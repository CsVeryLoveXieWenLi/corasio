/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/http/session.h"

#include <asio.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>


namespace corasio::http {
class server {
public:
    using handler = std::function<asio::awaitable<void>(request&, response&, session&)>;

    explicit server() = default;
    ~server() = default;

    server(const server&) = delete;
    server& operator=(const server&) = delete;

    server(server&&) = delete;
    server& operator=(server&&) = delete;

public:
    void set_read_timeout(std::chrono::milliseconds ms) noexcept;
    void set_write_timeout(std::chrono::milliseconds ms) noexcept;

    void set_buffer_size(std::size_t size) noexcept;

    void set_handler(handler fn) noexcept;

public:
    void run(std::string host, std::string port, std::uint16_t worker_num) noexcept;

private:
    asio::awaitable<void> listener(asio::ip::tcp::acceptor acceptor, std::vector<asio::io_context::executor_type>& executors) noexcept;
    asio::awaitable<void> echo(asio::ip::tcp::socket socket) noexcept;

private:
    handler handler_;

    std::chrono::milliseconds read_timeout_{5000};
    std::chrono::milliseconds write_timeout_{5000};

    std::size_t buffer_size_ = 128 * 1000;
};
} // namespace corasio::http
