/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: client（明文）与 tls_client（TLS）的测试
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "tests.h"

#include "http/client.h"
#include "http/enums.h"
#include "http/server.h"
#include "http/tls_client.h"

#include <asio.hpp>
#include <asio/ssl.hpp>

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

using namespace corasio;

namespace {
constexpr const char* client_host = "127.0.0.1";
constexpr const char* client_port = "18712";
constexpr const char* tls_port = "18713";
constexpr const char* cert_file = "tests/certs/server.crt";
constexpr const char* key_file = "tests/certs/server.key";

// 本地最小 TLS 服务器：握手 -> 读一个请求 -> 回一条固定响应
void run_tls_stub_server() {
    asio::io_context io;
    asio::error_code ec;

    asio::ssl::context ctx(asio::ssl::context::tls_server);
    ctx.use_certificate_chain_file(cert_file, ec);
    if (ec) {
        std::printf("    [tls stub] 证书读不到: %s\n", ec.message().c_str());
        return;
    }
    ctx.use_private_key_file(key_file, asio::ssl::context::pem, ec);
    if (ec) {
        std::printf("    [tls stub] 私钥读不到: %s\n", ec.message().c_str());
        return;
    }

    const auto endpoint = asio::ip::tcp::endpoint(asio::ip::make_address(client_host), static_cast<asio::ip::port_type>(std::atoi(tls_port)));

    asio::ip::tcp::acceptor acceptor(io);
    acceptor.open(endpoint.protocol(), ec);
    if (ec) {
        std::printf("    [tls stub] 打开监听失败: %s\n", ec.message().c_str());
        return;
    }
    acceptor.set_option(asio::socket_base::reuse_address(true), ec);
    acceptor.bind(endpoint, ec);
    if (ec) {
        std::printf("    [tls stub] 监听失败: %s\n", ec.message().c_str());
        return;
    }
    acceptor.listen(asio::socket_base::max_listen_connections, ec);
    if (ec) return;

    asio::ip::tcp::socket socket(io);
    acceptor.accept(socket, ec);
    if (ec) return;

    asio::ssl::stream<asio::ip::tcp::socket> tls(std::move(socket), ctx);


    tls.handshake(asio::ssl::stream_base::server, ec);
    if (ec) {
        std::printf("    [tls stub] 握手失败: %s\n", ec.message().c_str());
        return;
    }

    asio::streambuf request;
    asio::read_until(tls, request, "\r\n\r\n", ec);
    if (ec) return;

    const std::string body = "hello tls";
    const std::string response = "HTTP/1.1 200 OK\r\ncontent-length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
    asio::write(tls, asio::buffer(response), ec);

    // 给客户端一点时间读完，别直接 RST
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    asio::error_code ignored;
    tls.next_layer().shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
}
} // namespace

void test_client() {
    std::printf("\n[client]\n");

    asio::io_context io;

    http::server srv;
    srv.set_read_timeout(std::chrono::milliseconds(3000));
    srv.set_handler([](http::request& req, http::response& res, http::session& sess) -> asio::awaitable<void> {
        const std::string body = std::string(req.get_path());
        const std::string content_length = std::to_string(body.size());

        res.set_version(http::enums::version::v1_1);
        res.set_status_code(http::enums::status_code::ok);
        res.set_status_message(http::enums::status_message::ok);
        res.set_header("content-length", content_length);
        res.set_body(body);

        co_await sess.send(req, res);
    });
    std::thread([&srv] { srv.run(client_host, client_port, 1); }).detach();
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    struct outcome {
        bool connected = false;
        bool first = false;
        bool reused = false;
        bool second = false;
        std::string first_body;
        std::string second_body;
        std::string status;
        std::size_t header_count = 0;
    } result;

    asio::co_spawn(
        io,
        [&result]() -> asio::awaitable<void> {
            http::client c;
            c.set_read_timeout(std::chrono::milliseconds(3000));
            c.set_write_timeout(std::chrono::milliseconds(3000));
            c.set_buffer_size(4096);

            result.connected = co_await c.connect(client_host, client_port);
            if (!result.connected) co_return;

            auto& request = c.get_request();
            request.set_version(http::enums::version::v1_1);
            request.set_method("GET");
            request.set_path("/hello");
            request.set_header("host", "127.0.0.1:18712");

            result.first = co_await c.send();
            if (result.first) {
                auto& response = c.get_response();
                result.status.assign(response.get_status_code());
                result.first_body.assign(response.get_body());
                result.header_count = response.get_headers().size();
            }

            // 复用连接：同一个 hostname，connect 不该重连
            result.reused = co_await c.connect(client_host, client_port);

            c.get_request().set_path("/again");
            result.second = co_await c.send();
            if (result.second) result.second_body.assign(c.get_response().get_body());

            co_return;
        },
        asio::detached);

    io.run();

    std::printf("    status=%s body=\"%s\" headers=%zu\n", result.status.c_str(), result.first_body.c_str(), result.header_count);
    check(result.connected, "connect 成功");
    check(result.first, "send 收到响应");
    check(result.status == "200", "状态码 200");
    check(result.first_body == "/hello", "body 是请求路径");
    check(result.reused, "同一 hostname 复用连接");
    check(result.second, "复用连接上第二次 send");
    check(result.second_body == "/again", "第二次响应正确");
}

void test_tls_client() {
    std::printf("\n[tls_client]\n");

    std::thread stub(run_tls_stub_server);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    asio::io_context io;
    struct outcome {
        bool connected = false;
        bool send = false;
        std::string status;
        std::string body;
    } result;

    asio::co_spawn(
        io,
        [&result]() -> asio::awaitable<void> {
            http::tls_client c;
            c.set_read_timeout(std::chrono::milliseconds(5000));
            c.set_write_timeout(std::chrono::milliseconds(5000));
            c.set_buffer_size(4096);

            result.connected = co_await c.connect(client_host, tls_port);
            if (!result.connected) co_return;

            auto& request = c.get_request();
            request.set_version(http::enums::version::v1_1);
            request.set_method("GET");
            request.set_path("/");
            request.set_header("host", "127.0.0.1");

            result.send = co_await c.send();
            if (result.send) {
                auto& response = c.get_response();
                result.status.assign(response.get_status_code());
                result.body.assign(response.get_body());
            }

            co_return;
        },
        asio::detached);

    io.run();
    stub.join();

    std::printf("    status=%s body=\"%s\"\n", result.status.c_str(), result.body.c_str());
    check(result.connected, "TLS 握手成功");
    check(result.send, "send 收到响应");
    check(result.status == "200", "状态码 200");
    check(result.body == "hello tls", "body 正确（TLS 解密后的内容）");
}
