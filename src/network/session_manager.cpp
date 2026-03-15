#include "network/session_manager.h"
#include <iostream>
#include <cstring>

SessionManager::SessionManager(asio::io_context& ioc, const KcpConfig& cfg, KcpSession::SendCallback send_cb, MessageCallback msg_cb)
    : ioc_(ioc), config_(cfg), send_cb_(send_cb), msg_cb_(msg_cb)
{
    for (size_t i = 0; i < shard_count_; ++i) {
        shards_.emplace_back(std::make_unique<Shard>());
    }
}

uint32_t SessionManager::get_conv(const char* data) {
    uint32_t conv = 0;
    std::memcpy(&conv, data, 4);
    return conv;
}

void SessionManager::handle_input(const asio::ip::udp::endpoint& ep, const char* data, size_t len) {
    if (len < 24) return;

    uint32_t conv = get_conv(data);
    size_t shard_idx = conv % shard_count_;
    auto& shard = *shards_[shard_idx];
    
    KcpSession::Ptr session;
    
    {
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.sessions.find(conv);
        if (it != shard.sessions.end()) {
            session = it->second;
        }
    }

    if (!session) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        auto it = shard.sessions.find(conv);
        if (it != shard.sessions.end()) {
            session = it->second;
        } else {
            std::cout << "New session conv: " << conv << " from " << ep.address().to_string() << ":" << ep.port() << std::endl;
            // Create session with current config
            session = std::make_shared<KcpSession>(conv, ioc_, ep, config_, send_cb_);
            shard.sessions[conv] = session;
        }
    }

    if (session->remote_endpoint() != ep) {
        // Simple security check against session hijacking
        // In production, use token authentication
        std::cerr << "Session " << conv << " endpoint mismatch. Expected " 
                  << session->remote_endpoint().address().to_string() << ":" << session->remote_endpoint().port()
                  << ", got " << ep.address().to_string() << ":" << ep.port() << std::endl;
        return;
    }

    session->input(data, len);

    // Try receive application messages
    std::string buf;
    while (session->try_recv(buf)) {
        if (msg_cb_) {
            msg_cb_(session, buf);
        }
    }
}

void SessionManager::update_all(uint32_t current_ms) {
    for (auto& shard_ptr : shards_) {
        std::lock_guard<std::mutex> lock(shard_ptr->mutex);
        for (auto& pair : shard_ptr->sessions) {
            pair.second->update(current_ms);
        }
    }
}

void SessionManager::check_timeout(uint32_t current_ms, uint32_t timeout_ms) {
    for (auto& shard_ptr : shards_) {
        std::unique_lock<std::mutex> lock(shard_ptr->mutex);
        for (auto it = shard_ptr->sessions.begin(); it != shard_ptr->sessions.end();) {
            // Check last active time (30s default)
            if (current_ms > it->second->get_last_active() + timeout_ms) {
                std::cout << "Session " << it->first << " timeout" << std::endl;
                it = shard_ptr->sessions.erase(it);
            } else {
                ++it;
            }
        }
    }
}
