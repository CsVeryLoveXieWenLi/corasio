#include "corasio/websocket/handshake.h"

#include "corasio/detail/base64.h"
#include "corasio/detail/random.h"
#include "corasio/detail/sha1.h"
#include "corasio/detail/text.h"
#include "corasio/http/enums.h"

#include <cstddef>
#include <string>
#include <string_view>


namespace corasio::websocket::handshake {
namespace {
constexpr std::string_view guid = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
} // namespace

std::string make_key() {
    unsigned char bytes[16] = {};
    detail::random::fill(bytes, sizeof(bytes));

    std::string key;
    detail::base64::encode(std::string_view(reinterpret_cast<const char*>(bytes), sizeof(bytes)), key);
    return key;
}

std::string accept_for(std::string_view key) {
    std::string source;
    source.reserve(key.size() + guid.size());
    source.append(key);
    source.append(guid);

    const auto digest = detail::sha1::hash(source);
    const auto raw = std::string_view(reinterpret_cast<const char*>(digest.data()), digest.size());

    std::string accept;
    detail::base64::encode(raw, accept);
    return accept;
}

bool is_upgrade(http::request& request) noexcept {
    return detail::text::has_token(request.find_header("upgrade"), "websocket") &&
           detail::text::has_token(request.find_header("connection"), "upgrade") &&
           request.find_header("sec-websocket-version") == "13" && !request.find_header("sec-websocket-key").empty();
}

bool is_accepted(http::response& response, std::string_view key) noexcept {
    if (response.get_status_code() != http::enums::status_code::switching_protocols) return false;
    return response.find_header("sec-websocket-accept") == accept_for(key);
}
} // namespace corasio::websocket::handshake