/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "websocket/session.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::websocket {
namespace {
constexpr unsigned char control_mark = 0x08;
} // namespace

session::session(asio::ip::tcp::socket socket, detail::buffer buffer) noexcept : socket_(std::move(socket)), buffer_(std::move(buffer)) {};
asio::awaitable<bool> session::fill() noexcept {
    if (buffer_.write_size() == 0) co_return false; // 一帧比 buffer 还大，装不下

    const auto result =
        co_await socket_.async_read_some(asio::buffer(buffer_.write_pos(), buffer_.write_size()), asio::cancel_after(read_timeout_, asio::as_tuple));
    if (std::get<0>(result) || std::get<1>(result) == 0) co_return false;

    buffer_.commit(std::get<1>(result));
    co_return true;
}

asio::awaitable<bool> session::fill_frame(header& header, std::size_t& header_size) noexcept {
    while (true) {
        const auto view = buffer_.readable();
        const auto size = frame::parse(view, header);

        if (size == -1) co_return false;
        if (size > 0 && view.size() >= static_cast<std::size_t>(size) + header.payload_size) {
            header_size = static_cast<std::size_t>(size);
            co_return true;
        }

        if (!co_await fill()) co_return false;
    }
}

asio::awaitable<std::optional<message>> session::read() noexcept {
    if (closed_) co_return std::nullopt;

    enums::opcode assembling = enums::opcode::text;
    std::string payload;
    bool started = false;

    while (true) {
        header header;
        std::size_t header_size = 0;

        if (!co_await fill_frame(header, header_size)) {
            closed_ = true;
            co_return std::nullopt;
        }

        // 客户端发来的帧必须掩码
        if (!header.masked) {
            co_await close(enums::close_code::protocol_error);
            co_return std::nullopt;
        }

        std::string body(buffer_.readable().substr(header_size, header.payload_size));
        buffer_.consume(header_size + header.payload_size);

        frame::mask(body.data(), body.size(), header.mask_key, 0);

        if ((static_cast<unsigned char>(header.op) & control_mark) != 0) {
            if (header.op == enums::opcode::ping) {
                if (!co_await send(body, enums::opcode::pong)) {
                    closed_ = true;
                    co_return std::nullopt;
                }
                continue;
            }

            if (header.op == enums::opcode::pong) continue;

            // close：回一个同样的 close，然后把控制权交给调用方
            const bool sent = co_await send(body, enums::opcode::close);
            closed_ = true;
            if (!sent) co_return std::nullopt;

            co_return message{std::move(body), enums::opcode::close};
        }

        if (header.op == enums::opcode::continuation) {
            if (!started) {
                closed_ = true;
                co_return std::nullopt;
            }
            payload += body;
        } else {
            if (started) {
                closed_ = true;
                co_return std::nullopt;
            }
            started = true;
            assembling = header.op;
            payload = std::move(body);
        }

        if (!header.fin) continue;

        co_return message{std::move(payload), assembling};
    }
}

asio::awaitable<bool> session::send(std::string_view payload, enums::opcode op) noexcept {
    if (closed_) co_return false;

    header header;
    header.payload_size = payload.size();
    header.op = op;
    header.masked = false;
    header.fin = true;

    char head[14] = {};
    const auto head_size = frame::write(header, head);

    buffers_.clear();
    buffers_.push_back(asio::buffer(head, head_size));
    buffers_.push_back(asio::buffer(payload.data(), payload.size()));

    const auto written = co_await asio::async_write(socket_, buffers_, asio::cancel_after(write_timeout_, asio::as_tuple));

    co_return !std::get<0>(written);
}

asio::awaitable<void> session::close(enums::close_code code) noexcept {
    if (closed_) co_return;

    const auto value = static_cast<std::uint16_t>(code);
    const char payload[2] = {static_cast<char>(value >> 8), static_cast<char>(value & 0xFF)};

    co_await send(std::string_view(payload, sizeof(payload)), enums::opcode::close);
    closed_ = true;
}
} // namespace corasio::websocket
