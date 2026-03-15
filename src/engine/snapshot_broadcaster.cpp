#include "engine/snapshot_broadcaster.h"
#include "network/session_manager.h"
#include "core/msg_ids.h"
#include "base.pb.h"

namespace engine {

void broadcast_world_snapshot(SessionManager& session_mgr, uint32_t server_time, uint32_t seq) {
    kcp_server::WorldSnapshot snapshot;
    snapshot.set_seq(seq);
    snapshot.set_server_time(server_time);

    // 暂用 session conv 作为实体：每个连接 = 一个占位实体
    // TODO: 接入 Room/ECS 后从 Transform/Health 等组件构建
    session_mgr.for_each_session([&snapshot](const KcpSession::Ptr& session) {
        auto* entity = snapshot.add_entities();
        entity->set_entity_id(session->get_conv());
        entity->mutable_pos()->set_x(0);
        entity->mutable_pos()->set_y(0);
        entity->mutable_pos()->set_z(0);
        entity->mutable_vel()->set_x(0);
        entity->mutable_vel()->set_y(0);
        entity->mutable_vel()->set_z(0);
        entity->set_yaw(0);
        entity->set_hp(100);
        entity->set_state_flags(0);
    });

    if (snapshot.entities_size() == 0) return;

    std::string payload = snapshot.SerializeAsString();
    kcp_server::BaseMessage base;
    base.set_msg_id(msg_id::WorldSnapshot);
    base.set_payload(payload);
    std::string data = base.SerializeAsString();
    // std::cout << "snapshot: " << data << std::endl;
    session_mgr.broadcast(data.data(), data.size());
}

}  // namespace engine
