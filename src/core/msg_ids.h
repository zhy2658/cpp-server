#pragma once

#include <cstdint>

// MsgID 常量定义，与 protocol_design.md 分段一致
// 修改时只需改此文件，保证收发双方一致

namespace msg_id {

// System (0-99)
constexpr uint32_t Ping = 1;
constexpr uint32_t Pong = 2;

// Battle Sync (2000-2999)
constexpr uint32_t PlayerInput = 2000;
constexpr uint32_t WorldSnapshot = 2001;

// Battle Event (3000-3999)
constexpr uint32_t FireEvent = 3000;
constexpr uint32_t MeleeAttackEvent = 3001;

}  // namespace msg_id
