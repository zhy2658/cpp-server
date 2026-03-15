#pragma once
#include <asio.hpp>
#include <memory>
#include <array>
#include <atomic>
#include <thread>
#include <mutex>
#include "network/session_manager.h"
#include "core/config.h"

class UdpServer {
public:
    UdpServer(asio::io_context& ioc, const ServerConfig& config);
    ~UdpServer();

    void start();
    void stop();

private:
    void start_receive_loop();
    void do_send(const char* data, size_t len, const asio::ip::udp::endpoint& ep);
    void start_update_loop();

    asio::io_context& ioc_;
    std::unique_ptr<asio::ip::udp::socket> socket_;
    // asio::strand<asio::io_context::executor_type> socket_strand_;
    
    ServerConfig config_;
    std::unique_ptr<SessionManager> session_mgr_;
    std::thread recv_thread_;
    std::thread update_thread_;
    std::mutex send_mutex_;
    std::atomic<bool> running_{false};
};
