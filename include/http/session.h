/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "http/request.h"
#include "http/response.h"

#include <asio.hpp>

#include <chrono>
#include <vector>


namespace corasio::http {
class session {
public:
    explicit session(asio::ip::tcp::socket& socket, std::chrono::milliseconds& read_timeout, std::chrono::milliseconds& write_timeout);
    ~session() = default;

    session(const session&) = delete;
    session& operator=(const session&) = delete;

    session(session&&) = delete;
    session& operator=(session&&) = delete;

public:
    asio::awaitable<void> send(http::request& request, http::response& response) noexcept;

private:
    asio::ip::tcp::socket& socket_;
    std::chrono::milliseconds& read_timeout_;
    std::chrono::milliseconds& write_timeout_;

    std::vector<asio::const_buffer> buffer_;
};
} // namespace corasio::http