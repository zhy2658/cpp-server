#include "udp_server.h"
#include <iostream>
#include "dispatcher.h"
#include "utils.h"

UdpServer::UdpServer(asio::io_context& ioc, const ServerConfig& config)
    : ioc_(ioc), 
      socket_(ioc, asio::ip::udp::endpoint(asio::ip::make_address(config.ip), config.port)),
      socket_strand_(asio::make_strand(ioc)),
      config_(config),
      timer_(ioc)
{
    session_mgr_ = std::make_unique<SessionManager>(ioc, config.kcp, 
        [this](const char* data, size_t len, const asio::ip::udp::endpoint& ep) {
            do_send(data, len, ep);
        },
        [this](KcpSession::Ptr session, const std::string& data) {
            Dispatcher::instance().dispatch(session, data, ioc_);
        });
}

UdpServer::~UdpServer() {
    stop();
}

void UdpServer::start() {
    running_ = true;
    do_receive();
    start_timer();
}

void UdpServer::stop() {
    running_ = false;
    timer_.cancel();
    socket_.close();
}

void UdpServer::do_receive() {
    socket_.async_receive_from(asio::buffer(recv_buf_), remote_ep_,
        asio::bind_executor(socket_strand_, [this](std::error_code ec, std::size_t len) {
            if (!ec) {
                if (running_) {
                    session_mgr_->handle_input(remote_ep_, recv_buf_.data(), len);
                    do_receive();
                }
            } else {
                if (ec != asio::error::operation_aborted) {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                }
            }
        }));
}

void UdpServer::do_send(const char* data, size_t len, const asio::ip::udp::endpoint& ep) {
    // Copy data to shared_ptr to keep it alive for async operation
    auto buf = std::make_shared<std::string>(data, len);
    asio::post(socket_strand_, [this, buf, ep]() {
        if (!running_) return;
        
        // socket_.async_send_to is not thread safe on the same socket object
        // So we must protect it with strand (which we are currently in)
        socket_.async_send_to(asio::buffer(*buf), ep,
            [this, buf](std::error_code ec, std::size_t /*bytes_sent*/) {
                if (ec && ec != asio::error::operation_aborted) {
                    std::cerr << "Send error: " << ec.message() << std::endl;
                }
            });
    });
}

void UdpServer::start_timer() {
    timer_.expires_after(std::chrono::milliseconds(10));
    timer_.async_wait([this](std::error_code ec) {
        if (!ec && running_) {
            uint32_t now = current_ms();
            session_mgr_->update_all(now);
            session_mgr_->check_timeout(now, 30000); // 30s timeout
            start_timer();
        }
    });
}
