/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <asio.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <tuple>
#include <utility>


namespace corasio::http {
// TLS 版本在 tls_client.h 里特化
template <typename Socket>
struct stream_traits {
    static constexpr std::string_view default_port = "80";

    template <typename Slot>
    void make(asio::any_io_executor executor, Slot& slot) {
        slot.emplace(std::move(executor));
    };

    asio::awaitable<bool> handshake(Socket&, std::string_view) noexcept { co_return true; };
};

template <typename Socket>
class basic_stream {
public:
    using executor_type = asio::any_io_executor;

    static constexpr std::string_view default_port = stream_traits<Socket>::default_port;

public:
    asio::awaitable<bool> connect(std::string_view host, std::string_view port) noexcept {
        traits_.make(co_await asio::this_coro::executor, socket_);

        asio::ip::tcp::resolver resolver(co_await asio::this_coro::executor);
        const auto resolved = co_await resolver.async_resolve(host, port, asio::as_tuple);
        if (std::get<0>(resolved)) co_return false;

        const auto connected = co_await asio::async_connect(socket_->lowest_layer(), std::get<1>(resolved), asio::as_tuple);
        if (std::get<0>(connected)) co_return false;

        asio::error_code ignored;
        socket_->lowest_layer().set_option(asio::ip::tcp::no_delay(true), ignored);

        if (!co_await traits_.handshake(*socket_, host)) co_return false;

        co_return true;
    };

    void close() noexcept {
        if (!socket_.has_value()) return;

        asio::error_code ignored;
        socket_->lowest_layer().close(ignored);
        socket_.reset();
    };

    template <typename MutableBufferSequence, typename Token>
    auto async_read_some(const MutableBufferSequence& buffers, Token&& token) {
        return socket_->async_read_some(buffers, std::forward<Token>(token));
    };

    template <typename ConstBufferSequence, typename Token>
    auto async_write_some(const ConstBufferSequence& buffers, Token&& token) {
        return socket_->async_write_some(buffers, std::forward<Token>(token));
    };

    asio::any_io_executor get_executor() noexcept { return socket_->get_executor(); };

private:
    stream_traits<Socket> traits_ {};
    std::optional<Socket> socket_;
};

using plain_stream = basic_stream<asio::ip::tcp::socket>;
} // namespace corasio::http
