#include "detail/base64.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>


namespace corasio::detail::base64 {
namespace {
constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
} // namespace

void encode(std::string_view input, std::string& output) noexcept {
    std::size_t index = 0;

    for (; index + 3 <= input.size(); index += 3) {
        const auto chunk = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index])) << 16) |
                           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index + 1])) << 8) |
                           static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index + 2]));

        output.push_back(alphabet[(chunk >> 18) & 0x3F]);
        output.push_back(alphabet[(chunk >> 12) & 0x3F]);
        output.push_back(alphabet[(chunk >> 6) & 0x3F]);
        output.push_back(alphabet[chunk & 0x3F]);
    }

    const auto rest = input.size() - index;
    if (rest == 1) {
        const auto chunk = static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index])) << 16;

        output.push_back(alphabet[(chunk >> 18) & 0x3F]);
        output.push_back(alphabet[(chunk >> 12) & 0x3F]);
        output.push_back('=');
        output.push_back('=');
    } else if (rest == 2) {
        const auto chunk = (static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index])) << 16) |
                           (static_cast<std::uint32_t>(static_cast<std::uint8_t>(input[index + 1])) << 8);

        output.push_back(alphabet[(chunk >> 18) & 0x3F]);
        output.push_back(alphabet[(chunk >> 12) & 0x3F]);
        output.push_back(alphabet[(chunk >> 6) & 0x3F]);
        output.push_back('=');
    }
}
} // namespace corasio::detail::base64