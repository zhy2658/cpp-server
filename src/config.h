#pragma once
#include <cstdint>
#include <string>
#include <iostream>

//结构体：KcpConfig，用于配置KCP参数
struct KcpConfig {
    int nodelay = 1;
    int interval = 10;
    int resend = 2;
    int nc = 1;
    int sndwnd = 128;
    int rcvwnd = 128;
    int mtu = 1400;
};

//结构体：ServerConfig，用于配置服务器参数
struct ServerConfig {
    std::string ip = "0.0.0.0";
    uint16_t port = 8888;
    int thread_pool_size = 1;
    KcpConfig kcp;

    //函数：load，用于加载配置文件
    static ServerConfig load(const std::string& path) {
        // Simplified: Return defaults, ignore yaml for now
        std::cout << "Loading config (using defaults, YAML disabled)..." << std::endl;
        return ServerConfig{};
    }
};
