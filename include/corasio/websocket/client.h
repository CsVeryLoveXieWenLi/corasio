/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 客户端：自己发握手，自己分帧（客户端发的帧要掩码）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/detail/buffer.h"
#include "corasio/detail/random.h"
#include "corasio/detail/text.h"
#include "corasio/http/enums.h"
#include "corasio/http/request.h"
#include "corasio/http/response_parser.h"
#include "corasio/http/stream.h"
#include "corasio/websocket/enums.h"
#include "corasio/websocket/frame.h"
#include "corasio/websocket/handshake.h"
#include "corasio/websocket/message.h"

#include <asio.hpp>
#include <llhttp.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::websocket {
template <typename Stream>
class basic_client {
public:
    basic_client() = default;
    ~basic_client() = default;

    basic_client(const basic_client&) = delete;
    basic_client& operator=(const basic_client&) = delete;

    basic_client(basic_client&&) = delete;
    basic_client& operator=(basic_client&&) = delete;

public:
    void set_read_timeout(std::chrono::milliseconds ms) noexcept { read_timeout_ = ms; };
    void set_write_timeout(std::chrono::milliseconds ms) noexcept { write_timeout_ = ms; };
    void set_buffer_size(std::size_t size) noexcept { buffer_size_ = size; };

    // 握手请求，connect 之前可以改 path / 加头
    http::request& get_request() noexcept { return request_; };

    // 握手响应（101）
    http::response& get_response() noexcept { return parser_.get(); };

    // 建连 + 握手；hostname 可带端口（"host:port"），不带就按流的默认端口（明文 80 / TLS 443）
    asio::awaitable<bool> connect(std::string_view hostname) noexcept;

    // 读一条完整消息，失败/被关闭返回 nullopt
    asio::awaitable<std::optional<message>> read() noexcept;

    // 客户端发的帧每帧带新掩码
    asio::awaitable<bool> send(std::string_view payload, enums::opcode op = enums::opcode::text) noexcept;

    asio::awaitable<void> close(enums::close_code code = enums::close_code::normal) noexcept;

private:
    asio::awaitable<bool> fill() noexcept;
    asio::awaitable<bool> fill_frame(header& out, std::size_t& header_size) noexcept;
    asio::awaitable<bool> write_frame(std::string_view payload, enums::opcode op) noexcept;

private:
    std::optional<Stream> stream_;
    std::optional<detail::buffer> buffer_;
    std::vector<asio::const_buffer> buffers_;

    http::request request_;
    http::response_parser parser_;
    std::string key_;

    std::chrono::milliseconds read_timeout_{5000};
    std::chrono::milliseconds write_timeout_{5000};
    std::size_t buffer_size_ = 128 * 1000;
    bool closed_ = false;
};

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::fill() noexcept {
    if (buffer_->write_size() == 0) co_return false;

    const auto result = co_await stream_->async_read_some(asio::buffer(buffer_->write_pos(), buffer_->write_size()),
                                                          asio::cancel_after(read_timeout_, asio::as_tuple));
    if (std::get<0>(result) || std::get<1>(result) == 0) co_return false;

    buffer_->commit(std::get<1>(result));
    co_return true;
}

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::fill_frame(header& out, std::size_t& header_size) noexcept {
    while (true) {
        const auto view = buffer_->readable();
        const auto size = frame::parse(view, out);

        if (size == -1) co_return false;
        if (size > 0 && view.size() >= static_cast<std::size_t>(size) + out.payload_size) {
            header_size = static_cast<std::size_t>(size);
            co_return true;
        }

        if (!co_await fill()) co_return false;
    }
}

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::connect(std::string_view hostname) noexcept {
    if (stream_.has_value()) co_return true;

    const auto parts = detail::text::split_hostname(hostname);
    if (parts.first.empty()) co_return false;

    const auto host = parts.first;
    const auto port = parts.second.empty() ? std::string_view(Stream::default_port) : parts.second;

    stream_.emplace();
    buffer_.emplace(buffer_size_);

    if (!co_await stream_->connect(host, port)) {
        stream_.reset();
        buffer_.reset();
        co_return false;
    }

    key_ = handshake::make_key();

    request_.set_version(http::enums::version::v1_1);
    if (request_.get_method().empty()) request_.set_method(http::enums::method::get);
    if (request_.get_path().empty()) request_.set_path("/");
    request_.set_header("host", hostname); // 带端口就更准确
    request_.set_header("upgrade", "websocket");
    request_.set_header("connection", "Upgrade");
    request_.set_header("sec-websocket-key", key_);
    request_.set_header("sec-websocket-version", "13");

    buffers_.clear();
    request_.to(buffers_);

    const auto written = co_await asio::async_write(*stream_, buffers_, asio::cancel_after(write_timeout_, asio::as_tuple));
    if (std::get<0>(written)) {
        stream_.reset();
        buffer_.reset();
        co_return false;
    }

    // 读 101：parser 停在报文末尾，后面的字节是第一帧，留着
    parser_.reset();

    while (true) {
        while (!buffer_->readable().empty()) {
            const auto view = buffer_->readable();
            const auto err = parser_.execute(view);

            if (err == HPE_PAUSED) {
                const auto total = static_cast<std::size_t>(parser_.pos() - view.data());
                const bool accepted = handshake::is_accepted(parser_.get(), key_);

                buffer_->consume(total);

                if (!accepted) {
                    stream_.reset();
                    buffer_.reset();
                    co_return false;
                }

                co_return true;
            }

            if (err != HPE_OK) {
                stream_.reset();
                buffer_.reset();
                co_return false;
            }

            buffer_->advance(view.size());
            break;
        }

        if (!co_await fill()) {
            stream_.reset();
            buffer_.reset();
            co_return false;
        }
    }
}

template <typename Stream>
asio::awaitable<std::optional<message>> basic_client<Stream>::read() noexcept {
    if (!stream_.has_value() || closed_) co_return std::nullopt;

    enums::opcode assembling = enums::opcode::text;
    std::string payload;
    bool started = false;

    while (true) {
        header incoming;
        std::size_t header_size = 0;

        if (!co_await fill_frame(incoming, header_size)) {
            closed_ = true;
            co_return std::nullopt;
        }

        // 服务端发来的帧不能带掩码
        if (incoming.masked) {
            co_await close(enums::close_code::protocol_error);
            co_return std::nullopt;
        }

        std::string body(buffer_->readable().substr(header_size, incoming.payload_size));
        buffer_->consume(header_size + incoming.payload_size);

        if ((static_cast<unsigned char>(incoming.op) & 0x08) != 0) {
            if (incoming.op == enums::opcode::ping) {
                if (!co_await send(body, enums::opcode::pong)) {
                    closed_ = true;
                    co_return std::nullopt;
                }
                continue;
            }

            if (incoming.op == enums::opcode::pong) continue;

            const bool sent = co_await send(body, enums::opcode::close);
            closed_ = true;
            if (!sent) co_return std::nullopt;

            co_return message{std::move(body), enums::opcode::close};
        }

        if (incoming.op == enums::opcode::continuation) {
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
            assembling = incoming.op;
            payload = std::move(body);
        }

        if (!incoming.fin) continue;

        co_return message{std::move(payload), assembling};
    }
}

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::write_frame(std::string_view payload, enums::opcode op) noexcept {
    if (!stream_.has_value() || closed_) co_return false;

    unsigned char mask[4] = {};
    detail::random::fill(mask, sizeof(mask));

    header outgoing;
    outgoing.payload_size = payload.size();
    outgoing.op = op;
    outgoing.masked = true;
    outgoing.mask_key = static_cast<std::uint32_t>(mask[0]) | (static_cast<std::uint32_t>(mask[1]) << 8) |
                       (static_cast<std::uint32_t>(mask[2]) << 16) | (static_cast<std::uint32_t>(mask[3]) << 24);

    // 掩码是原地 XOR，调用方的 view 不能改，拷一份
    std::string body(payload);
    frame::mask(body.data(), body.size(), outgoing.mask_key, 0);

    char head[14] = {};
    const auto head_size = frame::write(outgoing, head);

    buffers_.clear();
    buffers_.push_back(asio::buffer(head, head_size));
    buffers_.push_back(asio::buffer(body));

    const auto written = co_await asio::async_write(*stream_, buffers_, asio::cancel_after(write_timeout_, asio::as_tuple));
    co_return !std::get<0>(written);
}

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::send(std::string_view payload, enums::opcode op) noexcept {
    co_return co_await write_frame(payload, op);
}

template <typename Stream>
asio::awaitable<void> basic_client<Stream>::close(enums::close_code code) noexcept {
    if (!stream_.has_value() || closed_) co_return;

    const auto value = static_cast<std::uint16_t>(code);
    const char payload[2] = {static_cast<char>(value >> 8), static_cast<char>(value & 0xFF)};

    co_await write_frame(std::string_view(payload, sizeof(payload)), enums::opcode::close);
    closed_ = true;
}

using client = basic_client<http::plain_stream>;

extern template class basic_client<http::plain_stream>;
} // namespace corasio::websocket
