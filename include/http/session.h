/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "detail/buffer.h"
#include "http/request.h"
#include "http/response.h"
#include "websocket/session.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>


namespace corasio::http {
class session {
public:
    explicit session(asio::ip::tcp::socket& socket,
                     detail::buffer& buffer,
                     std::chrono::milliseconds& read_timeout,
                     std::chrono::milliseconds& write_timeout) noexcept;
    ~session() = default;

    session(const session&) = delete;
    session& operator=(const session&) = delete;

    session(session&&) = delete;
    session& operator=(session&&) = delete;

public:
    asio::awaitable<void> send(http::request& request, http::response& response) noexcept;

    // 当前请求在缓冲里占了多少字节，之后剩的字节属于下一条报文
    void set_parsed_size(std::size_t size) noexcept { parsed_size_ = size; };

    // 升级成 WebSocket：先回 101，再把 socket 交出去。失败返回 nullptr
    asio::awaitable<websocket::session*> upgrade(http::request& request) noexcept;

    // 升级成功后从这里拿 ws 会话，服务端据此结束 HTTP 循环
    websocket::session* get_websocket() noexcept { return ws_.has_value() ? &*ws_ : nullptr; };

private:
    asio::ip::tcp::socket& socket_;
    detail::buffer& buffer_;
    std::chrono::milliseconds& read_timeout_;
    std::chrono::milliseconds& write_timeout_;

    std::vector<asio::const_buffer> buffers_;
    std::optional<websocket::session> ws_;
    std::size_t parsed_size_ = 0;
    bool upgraded_ = false;
};
} // namespace corasio::http
