#pragma once
#include <asio.hpp>
#include <memory>
#include <array>
#include "session_manager.h"
#include "config.h"

class UdpServer {
public:
    UdpServer(asio::io_context& ioc, const ServerConfig& config);
    ~UdpServer();

    void start();
    void stop();

private:
    void do_receive();
    // Thread-safe send
    void do_send(const char* data, size_t len, const asio::ip::udp::endpoint& ep);
    void start_timer();

    asio::io_context& ioc_;
    asio::ip::udp::socket socket_;
    asio::strand<asio::io_context::executor_type> socket_strand_;
    
    asio::ip::udp::endpoint remote_ep_;
    std::array<char, 65536> recv_buf_;
    
    ServerConfig config_;
    std::unique_ptr<SessionManager> session_mgr_;
    asio::steady_timer timer_;
    bool running_ = false;
};
