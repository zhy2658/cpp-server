#include "udp_server.h"
#include <iostream>
#include "dispatcher.h"
#include "utils.h"

UdpServer::UdpServer(asio::io_context& ioc, const ServerConfig& config)
    : ioc_(ioc), 
      config_(config)
{
    std::cout << "UdpServer ctor: initializing socket..." << std::endl;
    try {
        socket_ = std::make_unique<asio::ip::udp::socket>(ioc, asio::ip::udp::endpoint(asio::ip::make_address(config.ip), config.port));
        std::cout << "UdpServer ctor: socket created" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Socket init failed: " << e.what() << std::endl;
        throw;
    }

    session_mgr_ = std::make_unique<SessionManager>(ioc, config.kcp, 
        [this](const char* data, size_t len, const asio::ip::udp::endpoint& ep) {
            do_send(data, len, ep);
        },
        [this](KcpSession::Ptr session, const std::string& data) {
            Dispatcher::instance().dispatch(session, data, ioc_);
        });
    std::cout << "UdpServer ctor: session_mgr created" << std::endl;
}

UdpServer::~UdpServer() {
    stop();
}

void UdpServer::start() {
    running_.store(true);
    start_receive_loop();
    start_update_loop();
}

void UdpServer::stop() {
    running_.store(false);
    if (socket_ && socket_->is_open()) {
        std::error_code ec;
        socket_->close(ec);
    }
    if (recv_thread_.joinable()) {
        recv_thread_.join();
    }
    if (update_thread_.joinable()) {
        update_thread_.join();
    }
}

void UdpServer::start_receive_loop() {
    recv_thread_ = std::thread([this]() {
        std::array<char, 65536> recv_buf{};
        while (running_.load()) {
            asio::ip::udp::endpoint ep;
            std::error_code ec;
            std::size_t len = socket_->receive_from(asio::buffer(recv_buf), ep, 0, ec);
            if (!running_.load()) {
                break;
            }
            if (ec) {
                if (ec != asio::error::operation_aborted) {
                    std::cerr << "Receive error: " << ec.message() << std::endl;
                }
                continue;
            }
            if (len > 0) {
                session_mgr_->handle_input(ep, recv_buf.data(), len);
            }
        }
    });
}

void UdpServer::do_send(const char* data, size_t len, const asio::ip::udp::endpoint& ep) {
    if (!running_.load()) return;
    std::lock_guard<std::mutex> lock(send_mutex_);
    std::error_code ec;
    socket_->send_to(asio::buffer(data, len), ep, 0, ec);
    if (ec && ec != asio::error::operation_aborted) {
        std::cerr << "Send error: " << ec.message() << std::endl;
    }
}

void UdpServer::start_update_loop() {
    update_thread_ = std::thread([this]() {
        while (running_.load()) {
            asio::post(ioc_, [this]() {
                if (!running_) return;
                uint32_t now = current_ms();
                session_mgr_->update_all(now);
                session_mgr_->check_timeout(now, 30000);
            });
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}
