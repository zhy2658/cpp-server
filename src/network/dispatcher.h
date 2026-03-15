#pragma once
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <iostream>
#include <asio.hpp>
#include "base.pb.h"
#include "network/kcp_session.h"

class Dispatcher {
public:
    using Handler = std::function<void(KcpSession::Ptr, const kcp_server::BaseMessage&)>;

    static Dispatcher& instance() {
        static Dispatcher d;
        return d;
    }

    void register_handler(uint32_t msg_id, Handler h) {
        handlers_[msg_id] = h;
    }

    void dispatch(KcpSession::Ptr session, const std::string& data, asio::io_context& ioc) {
        // Parse protobuf
        auto msg = std::make_shared<kcp_server::BaseMessage>();
        if (!msg->ParseFromString(data)) {
            std::cerr << "Failed to parse BaseMessage from conv " << session->get_conv() << std::endl;
            return;
        }
        
        auto it = handlers_.find(msg->msg_id());
        if (it != handlers_.end()) {
            // Post to io_context (worker thread)
            // Capture session and message by value (shared_ptr) to ensure lifetime
            asio::post(ioc, [handler = it->second, session, msg]() {
                handler(session, *msg);
            });
        } else {
            std::cerr << "Unknown msg_id " << msg->msg_id() << " from conv " << session->get_conv() << std::endl;
        }
    }

private:
    std::unordered_map<uint32_t, Handler> handlers_;
};
