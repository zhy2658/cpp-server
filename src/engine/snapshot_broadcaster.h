#pragma once

#include <cstdint>

class SessionManager;

// Layer 3 (Engine) - 世界快照广播，20-30Hz 向房间内所有客户端推送
namespace engine {

// Battle Sync 2000-2999
void broadcast_world_snapshot(SessionManager& session_mgr, uint32_t server_time, uint32_t seq);

}  // namespace engine
