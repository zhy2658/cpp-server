#include "server/game_server.h"
#include "handlers/ping_handler.h"
#include "network/dispatcher.h"
#include <thread>

GameServer::GameServer(const std::string& config_path) {
    load_config(config_path);
    ioc_ = std::make_unique<asio::io_context>(config_.thread_pool_size);
    register_handlers();
    start_udp_server();
}

void GameServer::load_config(const std::string& path) {
    config_ = ServerConfig::load(path);
}

void GameServer::register_handlers() {
    Dispatcher::instance().register_handler(1, handlers::on_ping);
}

void GameServer::start_udp_server() {
    server_ = std::make_unique<UdpServer>(*ioc_, config_);
    server_->start();
}

void GameServer::run() {
    auto work_guard = asio::make_work_guard(*ioc_);

    if (config_.thread_pool_size <= 1) {
        ioc_->run();
    } else {
        std::vector<std::thread> threads;
        for (int i = 0; i < config_.thread_pool_size; ++i) {
            threads.emplace_back([this] { ioc_->run(); });
        }
        for (auto& t : threads) {
            t.join();
        }
    }
}
