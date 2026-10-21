/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/detail/buffer.h"
#include "corasio/http/request.h"
#include "corasio/http/response.h"
#include "corasio/websocket/session.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <memory>

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

    // 升级成 WebSocket：先回 101，再把 socket/buffer 打包成 ws 会话交出去。失败返回 nullptr
    asio::awaitable<std::unique_ptr<websocket::session>> upgrade(http::request& request) noexcept;

    // 升级后 HTTP 这边就作废了，服务端据此结束循环
    bool is_upgraded() noexcept { return upgraded_; };

private:
    asio::ip::tcp::socket& socket_;
    detail::buffer& buffer_;
    std::chrono::milliseconds& read_timeout_;
    std::chrono::milliseconds& write_timeout_;

    std::vector<asio::const_buffer> buffers_;
    std::size_t parsed_size_ = 0;
    bool upgraded_ = false;
};
} // namespace corasio::http
