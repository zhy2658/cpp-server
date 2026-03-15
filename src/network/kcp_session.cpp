#include "network/kcp_session.h"
#include <iostream>
#include "core/utils.h"

KcpSession::KcpSession(uint32_t conv, asio::io_context& ioc, const asio::ip::udp::endpoint& ep, const KcpConfig& cfg, SendCallback send_cb)
    : conv_(conv), remote_ep_(ep), send_cb_(send_cb), last_active_(current_ms())
{
    kcp_ = ikcp_create(conv, this);
    if (!kcp_) {
        throw std::runtime_error("ikcp_create failed");
    }
    kcp_->output = kcp_output;

    ikcp_nodelay(kcp_, cfg.nodelay, cfg.interval, cfg.resend, cfg.nc);
    ikcp_wndsize(kcp_, cfg.sndwnd, cfg.rcvwnd);
    ikcp_setmtu(kcp_, cfg.mtu);
}

KcpSession::~KcpSession() {
    if (kcp_) {
        ikcp_release(kcp_);
        kcp_ = nullptr;
    }
}

int KcpSession::kcp_output(const char* buf, int len, ikcpcb* kcp, void* user) {
    auto self = static_cast<KcpSession*>(user);
    if (self && self->send_cb_) {
        self->send_cb_(buf, len, self->remote_ep_);
    }
    return 0;
}

void KcpSession::input(const char* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ikcp_input(kcp_, data, size);
    last_active_ = current_ms();
}

void KcpSession::update(uint32_t current_ms) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ikcp_update(kcp_, current_ms);
}

void KcpSession::send(const char* data, size_t size) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int ret = ikcp_send(kcp_, data, size);
    if (ret < 0) {
        std::cerr << "ikcp_send failed: " << ret << std::endl;
    }
}

bool KcpSession::try_recv(std::string& out_buffer) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    int size = ikcp_peeksize(kcp_);
    if (size <= 0) return false;

    out_buffer.resize(size);
    int ret = ikcp_recv(kcp_, &out_buffer[0], size);
    if (ret > 0) {
        return true;
    }
    return false;
}

uint32_t KcpSession::check(uint32_t current_ms) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    return ikcp_check(kcp_, current_ms);
}
