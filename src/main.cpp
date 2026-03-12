#include <asio.hpp>
#include <iostream>
#include <thread>
#include "udp_server.h"
#include "dispatcher.h"
#include "base.pb.h"
#include "utils.h"

// 业务处理: Ping
void on_ping(KcpSession::Ptr session, const kcp_server::BaseMessage& base) {
    kcp_server::Ping ping;
    if (ping.ParseFromString(base.payload())) {
        std::cout << "Recv Ping from conv " << session->get_conv() 
                  << ": content=" << ping.content() 
                  << ", time=" << ping.send_time() << std::endl;

        // 回复 Pong
        kcp_server::Pong pong;
        pong.set_content("Pong: " + ping.content());
        pong.set_recv_time(current_ms());

        kcp_server::BaseMessage resp;
        resp.set_msg_id(2); // 假设 2 是 Pong
        resp.set_payload(pong.SerializeAsString());
        resp.set_seq(base.seq());

        std::string data = resp.SerializeAsString();
        session->send(data.data(), data.size());
    }
}

int main() {
    try {
        ServerConfig config;
        try {
            config = ServerConfig::load("config.yaml");
        } catch(const std::exception& e) {
            std::cerr << "Failed to load config: " << e.what() << std::endl;
            return 1;
        }
        
        std::cout << "Starting server on " << config.ip << ":" << config.port << std::endl;

        // Register handlers
        // Msg ID 1: Ping
        Dispatcher::instance().register_handler(1, on_ping);

        asio::io_context ioc(config.thread_pool_size);
        
        // Work guard to keep ioc running
        auto work_guard = asio::make_work_guard(ioc);

        UdpServer server(ioc, config);
        server.start();

        std::vector<std::thread> threads;
        for (int i = 0; i < config.thread_pool_size; ++i) {
            threads.emplace_back([&ioc] { ioc.run(); });
        }

        std::cout << "Server running with " << config.thread_pool_size << " threads..." << std::endl;

        for (auto& t : threads) t.join();

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
