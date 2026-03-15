#pragma once

#include "network/kcp_session.h"
#include "base.pb.h"

// Layer 2 (Network) - 心跳消息处理，不包含 Gameplay 逻辑

namespace handlers {

// System 0-99: Ping / Pong
void on_ping(KcpSession::Ptr session, const kcp_server::BaseMessage& base);

}  // namespace handlers
