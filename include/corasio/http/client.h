/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/detail/buffer.h"
#include "corasio/http/request.h"
#include "corasio/http/response.h"
#include "corasio/http/response_parser.h"
#include "corasio/http/stream.h"

#include <asio.hpp>
#include <llhttp.h>

#include <chrono>
#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>


namespace corasio::http {
// connect 的入口放在各自的子类里（明文和 TLS 传参方式不同），这里只收共同逻辑。
// 传进来的字符串由调用方保证生命周期
template <typename Stream>
class basic_client {
public:
    explicit basic_client() = default;
    ~basic_client() = default;

    basic_client(const basic_client&) = delete;
    basic_client& operator=(const basic_client&) = delete;

    basic_client(basic_client&&) = delete;
    basic_client& operator=(basic_client&&) = delete;

public:
    void set_read_timeout(std::chrono::milliseconds ms) noexcept { read_timeout_ = ms; };
    void set_write_timeout(std::chrono::milliseconds ms) noexcept { write_timeout_ = ms; };
    void set_buffer_size(std::size_t size) noexcept { buffer_size_ = size; };

    // 结果通过 get_response() 拿，有效到下一次 send/connect
    asio::awaitable<bool> send() noexcept;

    request& get_request() noexcept { return request_; };
    response& get_response() noexcept { return parser_.get(); };

protected:
    // tunnel_host 为空表示直连
    asio::awaitable<bool> do_connect(std::string_view host,
                                     std::string_view port,
                                     std::string_view tunnel_host,
                                     std::string_view tunnel_port) noexcept;

private:
    void close() noexcept;

private:
    std::optional<Stream> stream_;
    std::optional<detail::buffer> buffer_;

    std::string_view host_;
    std::string_view port_;
    std::string_view tunnel_host_;
    std::string_view tunnel_port_;

    request request_;
    response_parser parser_;
    std::vector<asio::const_buffer> buffers_;

    std::chrono::milliseconds read_timeout_{5000};
    std::chrono::milliseconds write_timeout_{5000};
    std::size_t buffer_size_ = 128 * 1000;
};

template <typename Stream>
void basic_client<Stream>::close() noexcept {
    if (!stream_.has_value()) return;

    stream_->close();
    stream_.reset();
};

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::do_connect(std::string_view host,
                                                       std::string_view port,
                                                       std::string_view tunnel_host,
                                                       std::string_view tunnel_port) noexcept {
    if (stream_.has_value() && host == host_ && port == port_ && tunnel_host == tunnel_host_ && tunnel_port == tunnel_port_) {
        co_return true;
    }

    close();
    if (host.empty()) co_return false;

    host_ = host;
    port_ = port;
    tunnel_host_ = tunnel_host;
    tunnel_port_ = tunnel_port;

    stream_.emplace();
    buffer_.emplace(buffer_size_);

    if (!co_await stream_->connect(host_, port_, tunnel_host_, tunnel_port_)) {
        close();
        co_return false;
    }

    co_return true;
};

template <typename Stream>
asio::awaitable<bool> basic_client<Stream>::send() noexcept {
    // 服务器断开了就重连一次，不重发已写出去的报文
    if (!stream_.has_value()) {
        if (host_.empty()) co_return false;
        if (!co_await do_connect(host_, port_, tunnel_host_, tunnel_port_)) co_return false;
    }

    buffers_.clear();
    request_.to(buffers_);

    const auto written = co_await asio::async_write(*stream_, buffers_, asio::cancel_after(write_timeout_, asio::as_tuple));
    if (std::get<0>(written)) {
        close();
        co_return false;
    }

    parser_.reset();
    buffer_->consume(buffer_->readable_size());

    while (true) {
        while (!buffer_->readable().empty()) {
            const auto view = buffer_->readable();
            const auto err = parser_.execute(view);

            if (err == HPE_PAUSED) {
                buffer_->consume(buffer_->readable_size());
                co_return true;
            }

            if (err != HPE_OK) {
                close();
                co_return false;
            }

            buffer_->advance(view.size());
            break;
        }

        if (buffer_->write_size() == 0) {
            close();
            co_return false;
        }

        const auto result = co_await stream_->async_read_some(asio::buffer(buffer_->write_pos(), buffer_->write_size()),
                                                              asio::cancel_after(read_timeout_, asio::as_tuple));

        if (std::get<0>(result) || std::get<1>(result) == 0) {
            close();
            co_return false;
        }

        buffer_->commit(std::get<1>(result));
    }
};

class client : public basic_client<plain_stream> {
public:
    // 走代理时把 request 的 path 设成完整 URL 即可，这里只是连对端
    asio::awaitable<bool> connect(std::string_view host, std::string_view port) noexcept {
        co_return co_await do_connect(host, port, {}, {});
    };
};

extern template class basic_client<plain_stream>;
} // namespace corasio::http
