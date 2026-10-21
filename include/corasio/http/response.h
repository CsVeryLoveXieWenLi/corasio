/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <asio/buffer.hpp>

#include <string_view>
#include <unordered_map>
#include <vector>


namespace corasio::http {
class response {
public:
    explicit response() = default;
    ~response() = default;

    response(const response&) = delete;
    response& operator=(const response&) = delete;

    response(response&&) = delete;
    response& operator=(response&&) = delete;

public:
    void set_version(std::string_view version) noexcept;
    void set_body(std::string_view body) noexcept;
    void set_header(std::string_view key, std::string_view value) noexcept;

    void set_status_code(std::string_view status_code) noexcept;
    void set_status_message(std::string_view status_message) noexcept;

    void set_keep_alive(bool keep_alive) noexcept;

public:
    std::unordered_map<std::string_view, std::string_view>& get_headers() noexcept;
    std::string_view find_header(std::string_view key) noexcept;

    std::string_view& get_version() noexcept;
    std::string_view& get_status_code() noexcept;
    std::string_view& get_status_message() noexcept;
    std::string_view& get_body() noexcept;

    bool get_keep_alive() noexcept;

    void to(std::vector<asio::const_buffer>& output) noexcept;
    void reset() noexcept;

private:
    std::string_view body_;
    std::unordered_map<std::string_view, std::string_view> headers_;

    std::string_view status_code_;
    std::string_view status_message_;
    std::string_view version_;

    bool keep_alive_ = false;
};
} // namespace corasio::http