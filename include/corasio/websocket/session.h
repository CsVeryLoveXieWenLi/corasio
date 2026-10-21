/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 服务端会话：升级成功后直接接管 socket
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/detail/buffer.h"
#include "corasio/websocket/enums.h"
#include "corasio/websocket/frame.h"
#include "corasio/websocket/message.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string>
#include <vector>


namespace corasio::websocket {
class session {
public:
    // buffer 整个搬进来（http 升级时连残留字节一起带过来）
    session(asio::ip::tcp::socket socket, detail::buffer buffer) noexcept;

    session(const session&) = delete;
    session& operator=(const session&) = delete;

    session(session&&) = delete;
    session& operator=(session&&) = delete;

public:
    void set_read_timeout(std::chrono::milliseconds ms) noexcept { read_timeout_ = ms; };
    void set_write_timeout(std::chrono::milliseconds ms) noexcept { write_timeout_ = ms; };

    // 读一条完整消息：自动拼分片、自动回 pong、控制帧内部消化。断线/非法帧/已关闭返回 nullopt
    asio::awaitable<std::optional<message>> read() noexcept;

    // 服务端发出去的帧不加掩码
    asio::awaitable<bool> send(std::string_view payload, enums::opcode op = enums::opcode::text) noexcept;

    asio::awaitable<void> close(enums::close_code code = enums::close_code::normal) noexcept;

private:
    // 让 buffer 里攒够一整帧
    asio::awaitable<bool> fill_frame(header& header, std::size_t& header_size) noexcept;

    asio::awaitable<bool> fill() noexcept;

private:
    asio::ip::tcp::socket socket_;
    detail::buffer buffer_;
    std::vector<asio::const_buffer> buffers_; // 帧头和 payload 拼一次 write

    std::chrono::milliseconds read_timeout_{5000};
    std::chrono::milliseconds write_timeout_{5000};
    bool closed_ = false;
};
} // namespace corasio::websocket
