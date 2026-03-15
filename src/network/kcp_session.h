#pragma once
#include <cstdint>
#include <memory>
#include <functional>
#include <string>
#include <mutex>
#include <asio.hpp>
#include "ikcp.h"
#include "core/config.h"

class KcpSession : public std::enable_shared_from_this<KcpSession> {
public:
    using Ptr = std::shared_ptr<KcpSession>;
    using SendCallback = std::function<void(const char*, size_t, const asio::ip::udp::endpoint&)>;

    KcpSession(uint32_t conv, asio::io_context& ioc, const asio::ip::udp::endpoint& ep, const KcpConfig& cfg, SendCallback send_cb);
    ~KcpSession();

    // 禁止拷贝
    KcpSession(const KcpSession&) = delete;
    KcpSession& operator=(const KcpSession&) = delete;

    // 处理 UDP 收到的数据 (KCP 协议包)
    void input(const char* data, size_t size);

    // KCP 时间驱动
    void update(uint32_t current_ms);

    // 发送应用层数据 (会被封装成 KCP 包)
    void send(const char* data, size_t size);
    
    // 尝试接收应用层数据 (从 KCP 解包)
    bool try_recv(std::string& out_buffer);

    uint32_t get_conv() const { return conv_; }
    uint32_t get_last_active() const { return last_active_; }
    void set_last_active(uint32_t ms) { last_active_ = ms; }
    
    const asio::ip::udp::endpoint& remote_endpoint() const { return remote_ep_; }

    // 获取下一次 update 的时间点 (ms)
    uint32_t check(uint32_t current_ms);

private:
    static int kcp_output(const char* buf, int len, ikcpcb* kcp, void* user);

    ikcpcb* kcp_ = nullptr;
    uint32_t conv_;
    asio::ip::udp::endpoint remote_ep_;
    SendCallback send_cb_;
    uint32_t last_active_;
    
    // 保护 KCP 结构体
    std::recursive_mutex mutex_; 
};
