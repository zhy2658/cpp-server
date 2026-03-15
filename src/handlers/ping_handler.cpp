#include "handlers/ping_handler.h"
#include "core/msg_ids.h"
#include "core/utils.h"

namespace handlers {

void on_ping(KcpSession::Ptr session, const kcp_server::BaseMessage& base) {
    kcp_server::Ping ping;
    if (!ping.ParseFromString(base.payload())) {
        return;
    }
    uint32_t now = current_ms();
    kcp_server::Pong pong;
    pong.set_client_time(ping.client_time());
    pong.set_server_recv(now);
    pong.set_server_send(now);

    kcp_server::BaseMessage resp;
    resp.set_msg_id(msg_id::Pong);
    resp.set_payload(pong.SerializeAsString());
    resp.set_seq(base.seq());

    std::string data = resp.SerializeAsString();
    session->send(data.data(), data.size());
}

}  // namespace handlers
