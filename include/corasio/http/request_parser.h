/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <llhttp.h>

#include <corasio/http/request.h>

#include <cstddef>
#include <string>
#include <string_view>


namespace corasio::http {
class request_parser {
public:
    explicit request_parser();
    ~request_parser() = default;

    request_parser(const request_parser&) = delete;
    request_parser& operator=(const request_parser&) = delete;

    request_parser(request_parser&&) = delete;
    request_parser& operator=(request_parser&&) = delete;

public:
    llhttp_errno_t execute(std::string_view data) noexcept;
    void reset() noexcept;

    // 报文边界：execute 返回 HPE_PAUSED 时指向本报文结尾，出错时指向出错位置
    const char* pos() noexcept;

    request& get() noexcept;

private:
    llhttp_t parser_{};
    llhttp_settings_t settings_{};

    std::string body_;
    bool chunked_ = false;

    std::string_view url_;

    std::string_view header_key_;
    std::string_view header_value_;

    request request_;

    static int on_method(llhttp_t* parser, const char* at, size_t length) noexcept;

    static int on_url(llhttp_t* parser, const char* at, size_t length) noexcept;
    static int on_url_complete(llhttp_t* parser) noexcept;

    static int on_version_complete(llhttp_t* parser) noexcept;

    static int on_body(llhttp_t* parser, const char* at, size_t length) noexcept;

    static int on_header_field(llhttp_t* parser, const char* at, size_t length) noexcept;
    static int on_header_field_complete(llhttp_t* parser) noexcept;
    static int on_header_value(llhttp_t* parser, const char* at, size_t length) noexcept;
    static int on_header_value_complete(llhttp_t* parser) noexcept;
    static int on_headers_complete(llhttp_t* parser) noexcept;

    static int on_message_complete(llhttp_t* parser) noexcept;
};
} // namespace corasio::http
