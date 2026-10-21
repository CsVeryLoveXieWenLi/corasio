/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 一条 WebSocket 消息（可能是多个分片拼出来的）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include "websocket/enums.h"

#include <string>


namespace corasio::websocket {
struct message {
    std::string payload;
    enums::opcode op = enums::opcode::text;
};
} // namespace corasio::websocket
