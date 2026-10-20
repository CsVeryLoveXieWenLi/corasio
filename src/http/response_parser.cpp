/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "http/response_parser.h"
#include "http/enums.h"

#include "detail/ascii.h"

#include <llhttp.h>

#include <cstddef>
#include <string>
#include <string_view>


namespace corasio::http {
response_parser::response_parser() {
    llhttp_settings_init(&settings_);

    settings_.on_status = &response_parser::on_status;

    settings_.on_version_complete = &response_parser::on_version_complete;

    settings_.on_body = &response_parser::on_body;

    settings_.on_header_field = &response_parser::on_header_field;
    settings_.on_header_field_complete = &response_parser::on_header_field_complete;
    settings_.on_header_value = &response_parser::on_header_value;
    settings_.on_header_value_complete = &response_parser::on_header_value_complete;
    settings_.on_headers_complete = &response_parser::on_headers_complete;

    settings_.on_message_complete = &response_parser::on_message_complete;

    llhttp_init(&parser_, HTTP_RESPONSE, &settings_);
    parser_.data = this;
};

llhttp_errno_t response_parser::execute(std::string_view data) noexcept {
    if (data.empty()) return HPE_OK;
    return llhttp_execute(&parser_, data.data(), data.size());
};
void response_parser::reset() noexcept {
    response_.reset();

    body_.clear();
    chunked_ = false;

    header_key_ = std::string_view{};
    header_value_ = std::string_view{};

    // 重新 init 而不是 resume：resume 不会清 parser 里的 flags（Connection 标志、版本等），
    // 上一个报文的 Connection: close 会带到下一个报文，keep-alive 就判断错了
    llhttp_init(&parser_, HTTP_RESPONSE, &settings_);
    parser_.data = this;
};

const char* response_parser::pos() noexcept { return llhttp_get_error_pos(&parser_); };

response& response_parser::get() noexcept { return response_; };

int response_parser::on_status(llhttp_t* parser, const char* at, size_t length) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto& response = self->response_;
    auto& text = response.get_status_message();

    if (text.empty()) response.set_status_code(std::string_view(at - 4, 3));

    response.set_status_message(text.empty() ? std::string_view(at, length) : std::string_view(text.data(), text.size() + length));
    return HPE_OK;
};

int response_parser::on_version_complete(llhttp_t* parser) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto major = llhttp_get_http_major(parser);
    auto minor = llhttp_get_http_minor(parser);

    if (major == 1 && minor == 1) {
        self->response_.set_version(enums::version::v1_1);
    } else if (major == 1 && minor == 0) {
        self->response_.set_version(enums::version::v1_0);
    }
    return HPE_OK;
};

int response_parser::on_header_field(llhttp_t* parser, const char* at, size_t length) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto& text = self->header_key_;
    self->header_key_ = text.empty() ? std::string_view(at, length) : std::string_view(text.data(), text.size() + length);
    return HPE_OK;
};
int response_parser::on_header_field_complete(llhttp_t* parser) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto& key = self->header_key_;
    detail::ascii::to_lower(const_cast<char*>(key.data()), key.size());
    return HPE_OK;
};

int response_parser::on_header_value(llhttp_t* parser, const char* at, size_t length) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto& text = self->header_value_;
    self->header_value_ = text.empty() ? std::string_view(at, length) : std::string_view(text.data(), text.size() + length);
    return HPE_OK;
};
int response_parser::on_header_value_complete(llhttp_t* parser) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    self->response_.set_header(self->header_key_, self->header_value_);
    self->header_key_ = std::string_view{};
    self->header_value_ = std::string_view{};
    return HPE_OK;
};

int response_parser::on_headers_complete(llhttp_t* parser) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    self->chunked_ = self->response_.find_header("content-length").empty(); // 没有明确声明，则按流式读
    return HPE_OK;
};

int response_parser::on_body(llhttp_t* parser, const char* at, size_t length) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    auto& response = self->response_;
    auto& text = response.get_body();
    if (self->chunked_) {
        self->body_.append(at, length);
    } else {
        response.set_body(text.empty() ? std::string_view(at, length) : std::string_view(text.data(), text.size() + length));
    }
    return HPE_OK;
};

int response_parser::on_message_complete(llhttp_t* parser) noexcept {
    auto* self = static_cast<response_parser*>(parser->data);
    if (self->chunked_) self->response_.set_body(self->body_);
    // 整个报文已经解析完，这时取 keep-alive 才是准的（llhttp 会看版本 + Connection 头）
    self->response_.set_keep_alive(llhttp_should_keep_alive(parser) != 0);
    return HPE_PAUSED;
};
} // namespace corasio::http
