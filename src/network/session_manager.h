#pragma once
#include <cstdint>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <memory>
#include <functional>
#include "network/kcp_session.h"

class SessionManager {
public:
    using MessageCallback = std::function<void(KcpSession::Ptr, const std::string&)>;

    SessionManager(asio::io_context& ioc, const KcpConfig& cfg, KcpSession::SendCallback send_cb, MessageCallback msg_cb);

    void handle_input(const asio::ip::udp::endpoint& ep, const char* data, size_t len);
    void update_all(uint32_t current_ms);
    void check_timeout(uint32_t current_ms, uint32_t timeout_ms);

    /// 向所有会话广播数据 (Layer 2)
    void broadcast(const char* data, size_t len);
    /// 遍历所有会话 (用于快照生成等)
    template<typename F>
    void for_each_session(F&& f) {
        for (auto& shard_ptr : shards_) {
            std::lock_guard<std::mutex> lock(shard_ptr->mutex);
            for (auto& pair : shard_ptr->sessions) {
                f(pair.second);
            }
        }
    }

private:
    uint32_t get_conv(const char* data);
    
    struct Shard {
        std::unordered_map<uint32_t, KcpSession::Ptr> sessions;
        std::mutex mutex;
    };

    std::vector<std::unique_ptr<Shard>> shards_;
    asio::io_context& ioc_;
    KcpConfig config_;
    KcpSession::SendCallback send_cb_;
    MessageCallback msg_cb_;
    const size_t shard_count_ = 32;
};
