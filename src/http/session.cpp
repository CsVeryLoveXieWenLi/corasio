/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "http/session.h"

#include "http/enums.h"
#include "websocket/handshake.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>


namespace corasio::http {
session::session(asio::ip::tcp::socket& socket,
                 detail::buffer& buffer,
                 std::chrono::milliseconds& read_timeout,
                 std::chrono::milliseconds& write_timeout) noexcept
: socket_(socket),
  buffer_(buffer),
  read_timeout_(read_timeout),
  write_timeout_(write_timeout) {};

asio::awaitable<void> session::send(http::request& request, http::response& response) noexcept {
    // 升级之后 HTTP 这边不能再写
    if (upgraded_) co_return;

    // 每次发送前清空切片，否则会连着上一个响应一起发出去
    buffers_.clear();
    response.to(buffers_);

    // HEAD / 204 / 304 不能带 body：response::to() 把 body 放在最后一个切片，摘掉即可
    if (!response.get_body().empty()) {
        const auto code = response.get_status_code();
        if (request.get_method() == "HEAD" || code == enums::status_code::no_content || code == enums::status_code::not_modified) {
            buffers_.pop_back();
        }
    }

    // 用 as_tuple 而不是 use_awaitable：写失败（对端断开/写超时）不该抛异常，noexcept 里抛会直接 terminate
    const auto result = co_await asio::async_write(socket_, buffers_, asio::cancel_after(write_timeout_, asio::as_tuple));
    if (std::get<0>(result)) co_return;
};

asio::awaitable<std::unique_ptr<websocket::session>> session::upgrade(http::request& request) noexcept {
    if (upgraded_ || !websocket::handshake::is_upgrade(request)) co_return nullptr;

    // accept 基于请求里的 key；这行字符串要活到 101 写出去
    const std::string accept = websocket::handshake::accept_for(request.find_header("sec-websocket-key"));

    http::response response;
    response.set_version(enums::version::v1_1);
    response.set_status_code(enums::status_code::switching_protocols);
    response.set_status_message(enums::status_message::switching_protocols);
    response.set_header("upgrade", "websocket");
    response.set_header("connection", "Upgrade");
    response.set_header("sec-websocket-accept", accept);

    co_await send(request, response);

    // 本条请求的字节丢掉，后面的残留字节跟着 buffer 一起交给 ws
    buffer_.consume(parsed_size_);

    auto ws = std::make_unique<websocket::session>(std::move(socket_), std::move(buffer_));
    ws->set_read_timeout(read_timeout_);
    ws->set_write_timeout(write_timeout_);

    upgraded_ = true;
    co_return std::move(ws);
};
} // namespace corasio::http
