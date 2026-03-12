#pragma once
#include <string>
#include <iostream>

struct KcpConfig {
    int nodelay = 1;
    int interval = 10;
    int resend = 2;
    int nc = 1;
    int sndwnd = 128;
    int rcvwnd = 128;
    int mtu = 1400;
};

struct ServerConfig {
    std::string ip = "0.0.0.0";
    uint16_t port = 8888;
    int thread_pool_size = 4;
    KcpConfig kcp;

    static ServerConfig load(const std::string& path) {
        // Simplified: Return defaults, ignore yaml for now
        std::cout << "Loading config (using defaults, YAML disabled)..." << std::endl;
        return ServerConfig{};
    }
};
