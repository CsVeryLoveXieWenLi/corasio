/***
 * @Author: CsVeryLoveXieWenLi
 * @Description:
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#include "corasio/websocket/frame.h"

#include <emmintrin.h>

#include <cstddef>
#include <cstdint>
#include <string_view>


namespace corasio::websocket::frame {
namespace {
constexpr unsigned char fin_mark = 0x80;
constexpr unsigned char mask_mark = 0x80;
constexpr unsigned char length_mark = 0x7F;
constexpr unsigned char control_mark = 0x08;
} // namespace

std::ptrdiff_t parse(std::string_view data, header& out) noexcept {
    if (data.size() < 2) return 0;

    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data());

    const bool fin = (bytes[0] & fin_mark) != 0;
    const auto op = static_cast<enums::opcode>(bytes[0] & 0x0F);
    const bool masked = (bytes[1] & mask_mark) != 0;

    // 没协商任何扩展，rsv 必须全 0
    if ((bytes[0] & 0x70) != 0) return -1;

    switch (op) {
        case enums::opcode::continuation:
        case enums::opcode::text:
        case enums::opcode::binary:
        case enums::opcode::close:
        case enums::opcode::ping:
        case enums::opcode::pong: break;
        default: return -1;
    }

    std::uint64_t length = bytes[1] & length_mark;
    std::size_t size = 2;

    if (length == 126) {
        if (data.size() < 4) return 0;

        length = (static_cast<std::uint64_t>(bytes[2]) << 8) | bytes[3];
        size = 4;
    } else if (length == 127) {
        if (data.size() < 10) return 0;

        length = 0;
        for (int index = 0; index < 8; ++index) length = (length << 8) | bytes[2 + index];
        size = 10;
    }

    if ((bytes[0] & control_mark) != 0 && (!fin || length > 125)) return -1;

    std::uint32_t key = 0;
    if (masked) {
        if (data.size() < size + 4) return 0;

        key = static_cast<std::uint32_t>(bytes[size]) | (static_cast<std::uint32_t>(bytes[size + 1]) << 8) |
              (static_cast<std::uint32_t>(bytes[size + 2]) << 16) | (static_cast<std::uint32_t>(bytes[size + 3]) << 24);
        size += 4;
    }

    out.fin = fin;
    out.masked = masked;
    out.op = op;
    out.payload_size = length;
    out.mask_key = key;

    return static_cast<std::ptrdiff_t>(size);
}

std::size_t write(const header& value, char* out) noexcept {
    auto* bytes = reinterpret_cast<unsigned char*>(out);

    bytes[0] = static_cast<unsigned char>((value.fin ? fin_mark : 0) | static_cast<unsigned char>(value.op));
    bytes[1] = value.masked ? mask_mark : 0;

    const auto length = value.payload_size;
    std::size_t size = 2;

    if (length <= 125) {
        bytes[1] |= static_cast<unsigned char>(length);
    } else if (length <= 0xFFFF) {
        bytes[1] |= 126;
        bytes[size++] = static_cast<unsigned char>(length >> 8);
        bytes[size++] = static_cast<unsigned char>(length);
    } else {
        bytes[1] |= 127;
        for (int index = 0; index < 8; ++index) bytes[size++] = static_cast<unsigned char>(length >> ((7 - index) * 8));
    }

    if (value.masked) {
        for (int index = 0; index < 4; ++index) bytes[size++] = static_cast<unsigned char>(value.mask_key >> (index * 8));
    }

    return size;
}

void mask(char* data, std::size_t size, std::uint32_t key, std::size_t offset) noexcept {
    if (size == 0) return;

    // 旋转到起点所在的那个字节，之后按 4 字节周期重复
    const auto shift = static_cast<unsigned>(offset & 3);
    const auto rotated = shift == 0 ? key : (key >> (shift * 8)) | (key << (32 - shift * 8));

    auto* cursor = reinterpret_cast<unsigned char*>(data);
    std::size_t left = size;

    // 16 是 4 的倍数，所以一串 16 字节块都能用同一个向量
    const auto pattern = _mm_set1_epi32(static_cast<int>(rotated));
    for (; left >= 16; left -= 16, cursor += 16) {
        _mm_storeu_si128(reinterpret_cast<__m128i*>(cursor), _mm_xor_si128(_mm_loadu_si128(reinterpret_cast<const __m128i*>(cursor)), pattern));
    }

    // 不足 16 字节的尾巴
    for (std::size_t index = 0; index < left; ++index) cursor[index] ^= static_cast<unsigned char>(rotated >> ((index & 3) * 8));
}
} // namespace corasio::websocket::frame
