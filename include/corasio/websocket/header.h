/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: WebSocket 帧头
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "corasio/websocket/enums.h"

#include <cstdint>


namespace corasio::websocket {
struct header {
    std::uint64_t payload_size = 0;
    std::uint32_t mask_key = 0; // 4 个掩码字节按内存顺序打包（memcpy 从 wire 读出来就是）
    enums::opcode op = enums::opcode::text;
    bool fin = true;
    bool masked = false;
};
} // namespace corasio::websocket
