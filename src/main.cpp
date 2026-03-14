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

// 业务处理: RPC
void on_rpc(KcpSession::Ptr session, const kcp_server::BaseMessage& base) {
    kcp_server::RpcRequest req;
    if (req.ParseFromString(base.payload())) {
        std::cout << "Recv RPC from conv " << session->get_conv() 
                  << ": method=" << req.method() 
                  << ", params=" << req.params() << std::endl;

        // Simulate processing
        kcp_server::RpcResponse resp;
        resp.set_method(req.method());
        
        if (req.method() == "TestApi") {
            resp.set_code(0);
            resp.set_result("Success: " + req.params());
        } else {
            resp.set_code(404);
            resp.set_error_message("Method not found: " + req.method());
        }

        kcp_server::BaseMessage base_resp;
        base_resp.set_msg_id(101); // 101 is RpcResponse
        base_resp.set_payload(resp.SerializeAsString());
        base_resp.set_seq(base.seq());

        std::string data = base_resp.SerializeAsString();
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
        std::cout << "Registering handlers..." << std::endl;
        Dispatcher::instance().register_handler(1, on_ping);
        Dispatcher::instance().register_handler(100, on_rpc);

        std::cout << "Creating io_context..." << std::endl;
        asio::io_context ioc(config.thread_pool_size);
        
        // Work guard to keep ioc running
        auto work_guard = asio::make_work_guard(ioc);

        std::cout << "Creating UdpServer..." << std::endl;
        UdpServer server(ioc, config);
        
        std::cout << "Starting UdpServer..." << std::endl;
        server.start();

        if (config.thread_pool_size <= 1) {
            std::cout << "Server running in main thread..." << std::endl;
            ioc.run();
        } else {
            std::vector<std::thread> threads;
            for (int i = 0; i < config.thread_pool_size; ++i) {
                threads.emplace_back([&ioc] { ioc.run(); });
            }

            std::cout << "Server running with " << config.thread_pool_size << " threads..." << std::endl;

            for (auto& t : threads) t.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
