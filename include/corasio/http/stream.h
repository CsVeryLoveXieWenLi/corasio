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

    // 明文不建隧道：连上代理直接发 absolute-form
    asio::awaitable<bool> connect_tunnel(Socket&, std::string_view, std::string_view) noexcept { co_return true; };


    asio::awaitable<bool> handshake(Socket&, std::string_view) noexcept { co_return true; };
};

template <typename Socket>
class basic_stream {
public:
    using executor_type = asio::any_io_executor;

    static constexpr std::string_view default_port = stream_traits<Socket>::default_port;

public:
    // tunnel_host 为空表示直连；非空则先让代理按它建隧道，再用它做 SNI
    asio::awaitable<bool> connect(std::string_view host,
                                  std::string_view port,
                                  std::string_view tunnel_host = {},
                                  std::string_view tunnel_port = {}) noexcept {
        traits_.make(co_await asio::this_coro::executor, socket_);

        asio::ip::tcp::resolver resolver(co_await asio::this_coro::executor);
        const auto resolved = co_await resolver.async_resolve(host, port, asio::as_tuple);
        if (std::get<0>(resolved)) co_return false;

        const auto connected = co_await asio::async_connect(socket_->lowest_layer(), std::get<1>(resolved), asio::as_tuple);
        if (std::get<0>(connected)) co_return false;

        asio::error_code ignored;
        socket_->lowest_layer().set_option(asio::ip::tcp::no_delay(true), ignored);

        const bool tunnel = !tunnel_host.empty();
        if (tunnel && !co_await traits_.connect_tunnel(*socket_, tunnel_host, tunnel_port)) co_return false;
        if (!co_await traits_.handshake(*socket_, tunnel ? tunnel_host : host)) co_return false;

        co_return true;
    };

    // 复用前只看本地状态，不额外发系统调用；对端关了的话写的时候会被发现
    bool is_open() noexcept { return socket_.has_value() && socket_->lowest_layer().is_open(); };
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
