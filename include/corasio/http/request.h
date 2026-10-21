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
class request {
public:
    explicit request() = default;
    ~request() = default;

    request(const request&) = delete;
    request& operator=(const request&) = delete;

    request(request&&) = delete;
    request& operator=(request&&) = delete;

public:
    void set_version(std::string_view version) noexcept;
    void set_body(std::string_view body) noexcept;
    void set_header(std::string_view key, std::string_view value) noexcept;

    void set_path(std::string_view path) noexcept;
    void set_query(std::string_view query) noexcept;
    void set_method(std::string_view method) noexcept;

    void set_keep_alive(bool keep_alive) noexcept;

public:
    std::unordered_map<std::string_view, std::string_view>& get_headers() noexcept;
    std::string_view find_header(std::string_view key) noexcept;

    std::string_view& get_path() noexcept;
    std::string_view& get_query() noexcept;
    std::string_view find_query(std::string_view key) noexcept;

    std::string_view& get_version() noexcept;
    std::string_view& get_method() noexcept;
    std::string_view& get_body() noexcept;

    bool get_keep_alive() noexcept;

    void to(std::vector<asio::const_buffer>& output) noexcept;
    void reset() noexcept;

private:
    std::string_view body_;
    std::unordered_map<std::string_view, std::string_view> headers_;

    std::string_view path_;
    std::string_view query_;
    std::string_view method_;
    std::string_view version_;

    bool keep_alive_ = false;
};
} // namespace corasio::http