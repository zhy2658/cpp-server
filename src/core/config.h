#pragma once
#include <cstdint>
#include <string>
#include <iostream>
#include <fstream>
#include <yaml-cpp/yaml.h>

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
        ServerConfig config;
        try {
            YAML::Node root = YAML::LoadFile(path);
            
            if (root["server"]) {
                auto server = root["server"];
                if (server["ip"]) config.ip = server["ip"].as<std::string>();
                if (server["port"]) config.port = server["port"].as<uint16_t>();
                if (server["thread_pool_size"]) config.thread_pool_size = server["thread_pool_size"].as<int>();
            }

            if (root["kcp"]) {
                auto kcp = root["kcp"];
                if (kcp["nodelay"]) config.kcp.nodelay = kcp["nodelay"].as<int>();
                if (kcp["interval"]) config.kcp.interval = kcp["interval"].as<int>();
                if (kcp["resend"]) config.kcp.resend = kcp["resend"].as<int>();
                if (kcp["nc"]) config.kcp.nc = kcp["nc"].as<int>();
                if (kcp["sndwnd"]) config.kcp.sndwnd = kcp["sndwnd"].as<int>();
                if (kcp["rcvwnd"]) config.kcp.rcvwnd = kcp["rcvwnd"].as<int>();
                if (kcp["mtu"]) config.kcp.mtu = kcp["mtu"].as<int>();
            }
            std::cout << "[Config] Loaded configuration from " << path << std::endl;
        } catch (const YAML::Exception& e) {
            std::cerr << "[Config] Failed to load " << path << ": " << e.what() << ". Using defaults." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Config] Error: " << e.what() << std::endl;
        }
        return config;
    }
};
