/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/http/response.h"

#include <asio/buffer.hpp>

#include <string_view>
#include <unordered_map>
#include <vector>


namespace corasio::http {
namespace {
static constexpr std::string_view sp = " ";
static constexpr std::string_view crlf = "\r\n";
static constexpr std::string_view colon_sp = ": ";
} // namespace

void response::set_version(std::string_view version) noexcept { version_ = version; };
void response::set_body(std::string_view body) noexcept { body_ = body; };
void response::set_header(std::string_view key, std::string_view value) noexcept { headers_.insert_or_assign(key, value); };

void response::set_status_code(std::string_view status_code) noexcept { status_code_ = status_code; };
void response::set_status_message(std::string_view status_message) noexcept { status_message_ = status_message; };
void response::set_keep_alive(bool keep_alive) noexcept { keep_alive_ = keep_alive; };

std::unordered_map<std::string_view, std::string_view>& response::get_headers() noexcept { return headers_; };
std::string_view response::find_header(std::string_view key) noexcept {
    const auto it = headers_.find(key);
    return it == headers_.end() ? std::string_view{} : it->second;
};

std::string_view& response::get_version() noexcept { return version_; };
std::string_view& response::get_status_code() noexcept { return status_code_; };
std::string_view& response::get_status_message() noexcept { return status_message_; };
std::string_view& response::get_body() noexcept { return body_; };

bool response::get_keep_alive() noexcept { return keep_alive_; };

void response::to(std::vector<asio::const_buffer>& output) noexcept {
    output.emplace_back(asio::buffer(version_));
    output.emplace_back(asio::buffer(sp));
    output.emplace_back(asio::buffer(status_code_));
    output.emplace_back(asio::buffer(sp));
    output.emplace_back(asio::buffer(status_message_));
    output.emplace_back(asio::buffer(crlf));

    for (const auto& [key, value] : headers_) {
        output.emplace_back(asio::buffer(key));
        output.emplace_back(asio::buffer(colon_sp));
        output.emplace_back(asio::buffer(value));
        output.emplace_back(asio::buffer(crlf));
    }
    output.emplace_back(asio::buffer(crlf));

    if (!body_.empty()) output.emplace_back(asio::buffer(body_));
};
void response::reset() noexcept {
    body_ = std::string_view{};
    status_code_ = std::string_view{};
    version_ = std::string_view{};
    status_message_ = std::string_view{};
    keep_alive_ = false;
    headers_.clear();
};
} // namespace corasio::http
