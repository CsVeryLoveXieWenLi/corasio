/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "detail/text.h"

#include <cstddef>
#include <string_view>
#include <utility>


namespace corasio::detail::text {
void to_lower(char* data, std::size_t size) noexcept {
    for (std::size_t index = 0; index < size; ++index) {
        if (data[index] >= 'A' && data[index] <= 'Z') data[index] = static_cast<char>(data[index] - 'A' + 'a');
    }
}

std::pair<std::string_view, std::string_view> split_hostname(std::string_view hostname) noexcept {
    if (!hostname.empty() && hostname.front() == '[') {
        const auto close = hostname.find(']');
        if (close == std::string_view::npos) return {hostname.substr(1), {}};

        const auto host = hostname.substr(1, close - 1);
        auto rest = hostname.substr(close + 1);
        if (!rest.empty() && rest.front() == ':') return {host, rest.substr(1)};
        return {host, {}};
    }

    const auto colon = hostname.rfind(':');
    if (colon == std::string_view::npos) return {hostname, {}};

    // 多个冒号 = 裸 IPv6，整段都算 host
    if (hostname.find(':') != colon) return {hostname, {}};

    return {hostname.substr(0, colon), hostname.substr(colon + 1)};
}
} // namespace corasio::detail::text
