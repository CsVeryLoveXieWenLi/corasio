/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <string_view>


namespace corasio::http::enums {
namespace method {
static constexpr std::string_view get = "GET";
static constexpr std::string_view head = "HEAD";
static constexpr std::string_view post = "POST";
static constexpr std::string_view put = "PUT";
static constexpr std::string_view delete_ = "DELETE";
static constexpr std::string_view connect = "CONNECT";
static constexpr std::string_view options = "OPTIONS";
static constexpr std::string_view trace = "TRACE";
static constexpr std::string_view patch = "PATCH";
} // namespace method

namespace status_code {
static constexpr std::string_view continue_ = "100";
static constexpr std::string_view switching_protocols = "101";
static constexpr std::string_view processing = "102";
static constexpr std::string_view early_hints = "103";
static constexpr std::string_view upload_resumption_supported = "104";

static constexpr std::string_view ok = "200";
static constexpr std::string_view created = "201";
static constexpr std::string_view accepted = "202";
static constexpr std::string_view non_authoritative_information = "203";
static constexpr std::string_view no_content = "204";
static constexpr std::string_view reset_content = "205";
static constexpr std::string_view partial_content = "206";
static constexpr std::string_view multi_status = "207";
static constexpr std::string_view already_reported = "208";
static constexpr std::string_view im_used = "226";

static constexpr std::string_view multiple_choices = "300";
static constexpr std::string_view moved_permanently = "301";
static constexpr std::string_view found = "302";
static constexpr std::string_view see_other = "303";
static constexpr std::string_view not_modified = "304";
static constexpr std::string_view use_proxy = "305";
static constexpr std::string_view temporary_redirect = "307";
static constexpr std::string_view permanent_redirect = "308";

static constexpr std::string_view bad_request = "400";
static constexpr std::string_view unauthorized = "401";
static constexpr std::string_view payment_required = "402";
static constexpr std::string_view forbidden = "403";
static constexpr std::string_view not_found = "404";
static constexpr std::string_view method_not_allowed = "405";
static constexpr std::string_view not_acceptable = "406";
static constexpr std::string_view proxy_authentication_required = "407";
static constexpr std::string_view request_timeout = "408";
static constexpr std::string_view conflict = "409";
static constexpr std::string_view gone = "410";
static constexpr std::string_view length_required = "411";
static constexpr std::string_view precondition_failed = "412";
static constexpr std::string_view content_too_large = "413";
static constexpr std::string_view uri_too_long = "414";
static constexpr std::string_view unsupported_media_type = "415";
static constexpr std::string_view range_not_satisfiable = "416";
static constexpr std::string_view expectation_failed = "417";

static constexpr std::string_view misdirected_request = "421";
static constexpr std::string_view unprocessable_content = "422";
static constexpr std::string_view locked = "423";
static constexpr std::string_view failed_dependency = "424";
static constexpr std::string_view too_early = "425";
static constexpr std::string_view upgrade_required = "426";
static constexpr std::string_view precondition_required = "428";
static constexpr std::string_view too_many_requests = "429";
static constexpr std::string_view request_header_fields_too_large = "431";
static constexpr std::string_view unavailable_for_legal_reasons = "451";

static constexpr std::string_view internal_server_error = "500";
static constexpr std::string_view not_implemented = "501";
static constexpr std::string_view bad_gateway = "502";
static constexpr std::string_view service_unavailable = "503";
static constexpr std::string_view gateway_timeout = "504";
static constexpr std::string_view http_version_not_supported = "505";
static constexpr std::string_view variant_also_negotiates = "506";
static constexpr std::string_view insufficient_storage = "507";
static constexpr std::string_view loop_detected = "508";
static constexpr std::string_view not_extended = "510";
static constexpr std::string_view network_authentication_required = "511";
} // namespace status_code

namespace status_message {
static constexpr std::string_view continue_ = "Continue";
static constexpr std::string_view switching_protocols = "Switching Protocols";
static constexpr std::string_view processing = "Processing";
static constexpr std::string_view early_hints = "Early Hints";
static constexpr std::string_view upload_resumption_supported = "Upload Resumption Supported";

static constexpr std::string_view ok = "OK";
static constexpr std::string_view created = "Created";
static constexpr std::string_view accepted = "Accepted";
static constexpr std::string_view non_authoritative_information = "Non-Authoritative Information";
static constexpr std::string_view no_content = "No Content";
static constexpr std::string_view reset_content = "Reset Content";
static constexpr std::string_view partial_content = "Partial Content";
static constexpr std::string_view multi_status = "Multi-Status";
static constexpr std::string_view already_reported = "Already Reported";
static constexpr std::string_view im_used = "IM Used";

static constexpr std::string_view multiple_choices = "Multiple Choices";
static constexpr std::string_view moved_permanently = "Moved Permanently";
static constexpr std::string_view found = "Found";
static constexpr std::string_view see_other = "See Other";
static constexpr std::string_view not_modified = "Not Modified";
static constexpr std::string_view use_proxy = "Use Proxy";
static constexpr std::string_view temporary_redirect = "Temporary Redirect";
static constexpr std::string_view permanent_redirect = "Permanent Redirect";

static constexpr std::string_view bad_request = "Bad Request";
static constexpr std::string_view unauthorized = "Unauthorized";
static constexpr std::string_view payment_required = "Payment Required";
static constexpr std::string_view forbidden = "Forbidden";
static constexpr std::string_view not_found = "Not Found";
static constexpr std::string_view method_not_allowed = "Method Not Allowed";
static constexpr std::string_view not_acceptable = "Not Acceptable";
static constexpr std::string_view proxy_authentication_required = "Proxy Authentication Required";
static constexpr std::string_view request_timeout = "Request Timeout";
static constexpr std::string_view conflict = "Conflict";
static constexpr std::string_view gone = "Gone";
static constexpr std::string_view length_required = "Length Required";
static constexpr std::string_view precondition_failed = "Precondition Failed";
static constexpr std::string_view content_too_large = "Content Too Large";
static constexpr std::string_view uri_too_long = "URI Too Long";
static constexpr std::string_view unsupported_media_type = "Unsupported Media Type";
static constexpr std::string_view range_not_satisfiable = "Range Not Satisfiable";
static constexpr std::string_view expectation_failed = "Expectation Failed";

static constexpr std::string_view misdirected_request = "Misdirected Request";
static constexpr std::string_view unprocessable_content = "Unprocessable Content";
static constexpr std::string_view locked = "Locked";
static constexpr std::string_view failed_dependency = "Failed Dependency";
static constexpr std::string_view too_early = "Too Early";
static constexpr std::string_view upgrade_required = "Upgrade Required";
static constexpr std::string_view precondition_required = "Precondition Required";
static constexpr std::string_view too_many_requests = "Too Many Requests";
static constexpr std::string_view request_header_fields_too_large = "Request Header Fields Too Large";
static constexpr std::string_view unavailable_for_legal_reasons = "Unavailable For Legal Reasons";

static constexpr std::string_view internal_server_error = "Internal Server Error";
static constexpr std::string_view not_implemented = "Not Implemented";
static constexpr std::string_view bad_gateway = "Bad Gateway";
static constexpr std::string_view service_unavailable = "Service Unavailable";
static constexpr std::string_view gateway_timeout = "Gateway Timeout";
static constexpr std::string_view http_version_not_supported = "HTTP Version Not Supported";
static constexpr std::string_view variant_also_negotiates = "Variant Also Negotiates";
static constexpr std::string_view insufficient_storage = "Insufficient Storage";
static constexpr std::string_view loop_detected = "Loop Detected";
static constexpr std::string_view not_extended = "Not Extended";
static constexpr std::string_view network_authentication_required = "Network Authentication Required";
} // namespace status_message

namespace version {
static constexpr std::string_view v1_1 = "HTTP/1.1";
static constexpr std::string_view v1_0 = "HTTP/1.0";
} // namespace version
} // namespace corasio::http::enums