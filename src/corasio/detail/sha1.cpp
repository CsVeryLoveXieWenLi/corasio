/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/detail/sha1.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>


namespace corasio::detail::sha1 {
namespace {
constexpr std::uint32_t rotate(std::uint32_t value, int bits) noexcept { return (value << bits) | (value >> (32 - bits)); }

void process(std::uint32_t (&state)[5], const std::uint8_t* block) noexcept {
    std::uint32_t words[80];

    for (int index = 0; index < 16; ++index) {
        words[index] = (static_cast<std::uint32_t>(block[index * 4]) << 24) | (static_cast<std::uint32_t>(block[index * 4 + 1]) << 16) |
                       (static_cast<std::uint32_t>(block[index * 4 + 2]) << 8) | static_cast<std::uint32_t>(block[index * 4 + 3]);
    }
    for (int index = 16; index < 80; ++index) {
        words[index] = rotate(words[index - 3] ^ words[index - 8] ^ words[index - 14] ^ words[index - 16], 1);
    }

    auto a = state[0];
    auto b = state[1];
    auto c = state[2];
    auto d = state[3];
    auto e = state[4];

    for (int index = 0; index < 80; ++index) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;

        if (index < 20) {
            f = (b & c) | (~b & d);
            k = 0x5A827999;
        } else if (index < 40) {
            f = b ^ c ^ d;
            k = 0x6ED9EBA1;
        } else if (index < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8F1BBCDC;
        } else {
            f = b ^ c ^ d;
            k = 0xCA62C1D6;
        }

        const auto temp = rotate(a, 5) + f + e + k + words[index];
        e = d;
        d = c;
        c = rotate(b, 30);
        b = a;
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
}
} // namespace

std::array<std::uint8_t, 20> hash(std::string_view data) noexcept {
    std::uint32_t state[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};

    const auto* bytes = reinterpret_cast<const std::uint8_t*>(data.data());
    auto left = data.size();

    while (left >= 64) {
        process(state, bytes);
        bytes += 64;
        left -= 64;
    }

    std::uint8_t block[128] = {};
    for (std::size_t index = 0; index < left; ++index) block[index] = bytes[index];
    block[left] = 0x80;

    const std::size_t size = left < 56 ? 64 : 128;
    const auto bits = static_cast<std::uint64_t>(data.size()) * 8;
    for (std::size_t index = 0; index < 8; ++index) block[size - 1 - index] = static_cast<std::uint8_t>(bits >> (index * 8));

    process(state, block);
    if (size == 128) process(state, block + 64);

    std::array<std::uint8_t, 20> result{};
    for (std::size_t index = 0; index < 5; ++index) {
        result[index * 4] = static_cast<std::uint8_t>(state[index] >> 24);
        result[index * 4 + 1] = static_cast<std::uint8_t>(state[index] >> 16);
        result[index * 4 + 2] = static_cast<std::uint8_t>(state[index] >> 8);
        result[index * 4 + 3] = static_cast<std::uint8_t>(state[index]);
    }

    return result;
}
} // namespace corasio::detail::sha1
