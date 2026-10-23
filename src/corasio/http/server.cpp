/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/http/server.h"

#include "corasio/detail/buffer.h"

#include "corasio/http/request_parser.h"
#include "corasio/http/response.h"
#include "corasio/http/session.h"

#include <asio.hpp>
#include <asio/config.hpp>
#include <llhttp.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::http {
void server::set_read_timeout(std::chrono::milliseconds ms) noexcept { read_timeout_ = ms; };
void server::set_write_timeout(std::chrono::milliseconds ms) noexcept { write_timeout_ = ms; };

void server::set_buffer_size(std::size_t size) noexcept { buffer_size_ = size; };

void server::set_handler(handler fn) noexcept { handler_ = std::move(fn); };

asio::awaitable<void> server::listener(asio::ip::tcp::acceptor acceptor, std::vector<asio::io_context::executor_type>& executors) noexcept {
    std::size_t next = 0;
    std::size_t worker_num = executors.size() - 1;

    while (true) {
        auto target = executors[1 + (next++ % worker_num)];

        auto [ec, socket] = co_await acceptor.async_accept(target, asio::as_tuple);
        if (ec) {
            if (ec == asio::error::would_block || ec == asio::error::try_again || ec == asio::error::interrupted) continue;
            co_return;
        }

        asio::error_code ignored;
        socket.set_option(asio::ip::tcp::no_delay(true), ignored);
        asio::co_spawn(target, echo(std::move(socket)), asio::detached);
    }
};

asio::awaitable<void> server::echo(asio::ip::tcp::socket socket) noexcept {
    // 每条连接从内存池取一块当读写窗口，连接以任何方式结束都会自动还回去
    detail::buffer buffer(buffer_size_);

    request_parser parser;
    response response;
    session sess(socket, buffer, read_timeout_, write_timeout_);

    auto& request = parser.get();

    while (true) {
        // 1) 把缓冲区里还没喂过的新字节交给解析器。一次 read 可能带多个请求，循环到解析不动为止
        while (!buffer.readable().empty()) {
            auto view = buffer.readable();
            auto err = parser.execute(view);

            if (err == HPE_PAUSED) {
                // 一个完整请求：解析器停在报文末尾，停下的位置就是本请求占用的字节数
                const char* stop = parser.pos();
                if (stop == nullptr) co_return;

                std::size_t total = static_cast<std::size_t>(stop - view.data());
                bool keep_alive = request.get_keep_alive();

                // handler 自己决定发不发响应
                sess.set_parsed_size(total);
                if (handler_) co_await handler_(request, response, sess);

                // 升级成 WebSocket 了：这条连接交给 ws 会话，HTTP 循环到此为止
                if (sess.is_upgraded()) co_return;

                parser.reset();
                response.reset();

                // 请求处理完，缓冲区立刻可复用（内部把下一条报文的字节搬到开头）
                buffer.consume(total);

                if (!keep_alive) co_return;
                continue;
            }

            // 报文非法：直接断开，不回任何东西
            if (err != HPE_OK) co_return;

            buffer.advance(view.size()); // 全部喂完，还缺数据
            break;
        }

        // 2) 整块都塞满了还没有完整报文：单条报文超出窗口，直接断开
        if (buffer.write_size() == 0) co_return;

        // 3) 读新数据；读超时就取消这次读
        auto result = co_await socket.async_read_some(
            asio::buffer(buffer.write_pos(), buffer.write_size()),
            asio::cancel_after(read_timeout_, asio::as_tuple)
        );

        auto& ec = std::get<0>(result);
        auto n = std::get<1>(result);
        if (ec || n == 0) co_return;
        buffer.commit(n);
    }
};

void server::run(std::string host, std::string port, std::uint16_t worker_num) noexcept {
    // 0 号上下文跑接收线程，1..worker_num 跑工作线程，总线程数 = worker + 1
    std::size_t workers = worker_num == 0 ? 1 : static_cast<std::size_t>(worker_num);
    std::size_t total = workers + 1;

    std::vector<std::unique_ptr<asio::io_context>> contexts;
    std::vector<asio::io_context::executor_type> executors;
    std::vector<asio::executor_work_guard<asio::io_context::executor_type>> guards;
    contexts.reserve(total);
    executors.reserve(total);
    guards.reserve(total);

    // asio 默认 resolver 只有 1 条工作线程，DNS 会串行排队；
    // 这里按工作线程数给 resolver 配同样多的线程，高并发建连时才不会互相堵
    const std::string resolver_config = "resolver.threads=" + std::to_string(workers);

    for (std::size_t index = 0; index < total; ++index) {
        contexts.push_back(std::make_unique<asio::io_context>(asio::config_from_string{resolver_config}));
        executors.push_back(contexts.back()->get_executor());
        guards.push_back(asio::make_work_guard(executors.back()));
    }

    asio::error_code ec;
    auto address = asio::ip::make_address(host, ec);
    if (ec) return;

    auto endpoint = asio::ip::tcp::endpoint(address, static_cast<asio::ip::port_type>(std::strtoul(port.c_str(), nullptr, 10)));

    asio::ip::tcp::acceptor acceptor(*contexts[0]);
    acceptor.open(endpoint.protocol(), ec);
    if (ec) return;
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    if (ec) return;
    acceptor.bind(endpoint, ec);
    if (ec) return;
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) return;

    asio::co_spawn(executors[0], listener(std::move(acceptor), executors), asio::detached);

    // 接收线程和工作线程都新开：0 号跑接收，1..worker_num 跑工作线程
    std::vector<std::thread> threads;
    threads.reserve(total);
    for (std::size_t index = 0; index < total; ++index) {
        auto* context = contexts[index].get();
        threads.emplace_back([context] { context->run(); });
    }

    // 正常情况下各线程会一直阻塞在自己的 run() 里，这里只是等它们
    for (auto& thread : threads) thread.join();
};
} // namespace corasio::http
