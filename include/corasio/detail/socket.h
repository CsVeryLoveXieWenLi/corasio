/***
 * @Author: CsVeryLoveXieWenLi
 * @Description: 查连接是否已被对端关闭（复用连接前用，尽量少花系统调用）
 * @@Copyright (c) 2026 by CsVeryLoveXieWenLi, All Rights Reserved.
 */
#pragma once

#include <cstdint>


namespace corasio::detail::socket {
// 传 native_handle；健康空闲连接只会多一次 select，不发包、不消费数据、不改状态
bool is_peer_closed(std::intptr_t handle) noexcept;
} // namespace corasio::detail::socket
