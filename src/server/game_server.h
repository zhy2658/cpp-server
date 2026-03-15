#pragma once

#ifdef _WIN32
#include "core/win_socket_init.h"
#endif
#include <memory>
#include <asio.hpp>
#include "core/config.h"
#include "network/udp_server.h"

// 服务器引导与运行封装，整合 Config / io_context / UdpServer / 事件循环

class GameServer {
public:
    explicit GameServer(const std::string& config_path = "config.yaml");
    ~GameServer() = default;

    // 启动并阻塞运行，直到服务器退出
    void run();

private:
    ServerConfig config_;
    std::unique_ptr<asio::io_context> ioc_;
    std::unique_ptr<UdpServer> server_;

    void load_config(const std::string& path);
    void register_handlers();
    void start_udp_server();
};
