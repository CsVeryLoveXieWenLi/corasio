/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 的操作码与关闭码
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstdint>


namespace corasio::websocket::enums {
enum class opcode : unsigned char {
    continuation = 0x0,
    text = 0x1,
    binary = 0x2,
    close = 0x8,
    ping = 0x9,
    pong = 0xA,
};

enum class close_code : std::uint16_t {
    normal = 1000,
    going_away = 1001,
    protocol_error = 1002,
    unsupported_data = 1003,
    invalid_payload = 1007,
    policy_violation = 1008,
    message_too_big = 1009,
    internal_error = 1011,
};
} // namespace corasio::websocket::enums
