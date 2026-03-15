#include <iostream>
#include <thread>
#include <atomic>
#include <cmath>
#include <asio.hpp>
#include "network/kcp_session.h"
#include "core/utils.h"
#include "base.pb.h"
#include "core/msg_ids.h"

using asio::ip::udp;

class ConsoleClient {
public:
    ConsoleClient(const std::string& ip, uint16_t port, uint32_t conv, uint32_t target_msg_id) 
        : conv_(conv), target_msg_id_(target_msg_id)
    {
        udp::endpoint ep(asio::ip::make_address(ip), port);
        
        // 1. 创建 UDP Socket
        socket_ = std::make_unique<udp::socket>(ioc_, udp::v4());
        // Fix: Windows 下必须显式 bind 才能在 send_to 之前调用 receive_from，否则会报 "无效的参数" 错误
        socket_->bind(udp::endpoint(udp::v4(), 0));
        
        // 2. 创建 KcpSession
        KcpConfig config; // 使用默认配置
        config.interval = 10;
        config.nodelay = true;
        config.resend = 2;
        config.nc = true;

        session_ = std::make_shared<KcpSession>(conv, ioc_, ep, config, 
            [this](const char* data, size_t len, const udp::endpoint& ep) {
                // 发送回调
                std::error_code ec;
                socket_->send_to(asio::buffer(data, len), ep, 0, ec);
                if (ec) {
                    std::cerr << "Send error: " << ec.message() << std::endl;
                }
            });
    }

    void start() {
        running_ = true;
        
        // 启动接收线程
        recv_thread_ = std::thread([this]() {
            std::array<char, 65536> recv_buf;
            while (running_) {
                udp::endpoint sender_ep;
                std::error_code ec;
                size_t len = socket_->receive_from(asio::buffer(recv_buf), sender_ep, 0, ec);
                
                if (ec) {
                    if (ec != asio::error::operation_aborted)
                        std::cerr << "Receive error: " << ec.message() << std::endl;
                    continue;
                }

                // 将数据输入 KCP
                if (len > 0) {
                    session_->input(recv_buf.data(), len);
                }
            }
        });

        // 启动主循环 (Update & Input)
        loop_thread_ = std::thread([this]() {
            while (running_) {
                uint32_t now = current_ms();
                
                // KCP Update
                session_->update(now);

                // 尝试读取消息
                std::string msg_data;
                while (session_->try_recv(msg_data)) {
                    process_message(msg_data);
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });

        // 发送循环
        send_loop();
    }

    void stop() {
        running_ = false;
        socket_->close();
        if (recv_thread_.joinable()) recv_thread_.join();
        if (loop_thread_.joinable()) loop_thread_.join();
    }

private:
    void send_loop() {
        while (running_) {
            kcp_server::BaseMessage base;
            std::string log_msg;

            // 总是发送 Ping (保活)
            // 如果目标是 Ping，则只发送 Ping
            // 如果目标是其他，则额外发送其他包
            
            // 1. 发送 Ping
            {
                kcp_server::Ping ping;
                ping.set_client_time(current_ms());
                
                kcp_server::BaseMessage ping_base;
                ping_base.set_msg_id(msg_id::Ping);
                ping_base.set_payload(ping.SerializeAsString());
                
                std::string data = ping_base.SerializeAsString();
                session_->send(data.data(), data.size());
            }

            // 2. 发送目标包 (如果是 Ping 就不重复发了)
            if (target_msg_id_ != msg_id::Ping) {
                base.set_msg_id(target_msg_id_);
                
                if (target_msg_id_ == msg_id::PlayerInput) {
                    kcp_server::PlayerInput input;
                    input.set_frame_id(frame_count_++);
                    // 模拟绕圈移动
                    float time = current_ms() / 1000.0f;
                    input.mutable_move_dir()->set_x(std::sin(time));
                    input.mutable_move_dir()->set_y(std::cos(time));
                    input.mutable_move_dir()->set_z(0);
                    input.set_yaw(time * 10.0f);
                    base.set_payload(input.SerializeAsString());
                    log_msg = "Sent PlayerInput";
                }
                else if (target_msg_id_ == msg_id::FireEvent) {
                    kcp_server::FireEvent fire;
                    fire.set_shooter_id(1002);
                    fire.set_weapon_id(1);
                    fire.mutable_direction()->set_x(1.0f);
                    base.set_payload(fire.SerializeAsString());
                    log_msg = "Sent FireEvent";
                }
                
                if (!log_msg.empty()) {
                    std::string data = base.SerializeAsString();
                    session_->send(data.data(), data.size());
                    std::cout << "[Client] " << log_msg << std::endl;
                }
            } else {
                 std::cout << "[Client] Sent Ping" << std::endl;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 10Hz
        }
    }

    void process_message(const std::string& data) {
        kcp_server::BaseMessage base;
        if (!base.ParseFromString(data)) {
            std::cerr << "Failed to parse BaseMessage" << std::endl;
            return;
        }

        switch (base.msg_id()) {
            case msg_id::Pong: {
                kcp_server::Pong pong;
                pong.ParseFromString(base.payload());
                uint32_t rtt = current_ms() - pong.client_time();
                std::cout << "[Client] Recv Pong | RTT: " << rtt << "ms | ServerTime: " << pong.server_send() << std::endl;
                break;
            }
            case msg_id::WorldSnapshot: {
                kcp_server::WorldSnapshot snapshot;
                snapshot.ParseFromString(base.payload());
                std::cout << "[Client] WorldSnapshot | Seq: " << snapshot.seq() 
                          << " | Time: " << snapshot.server_time()
                          << " | Entities: " << snapshot.entities_size() << std::endl;
                
                for (const auto& entity : snapshot.entities()) {
                    std::cout << "  - Entity ID: " << entity.entity_id()
                              << " | HP: " << entity.hp()
                              << " | Pos: (" << entity.pos().x() << ", " 
                                           << entity.pos().y() << ", " 
                                           << entity.pos().z() << ")"
                              << " | Yaw: " << entity.yaw()
                              << std::endl;
                }
                break;
            }
            default:
                std::cout << "[Client] Unknown MsgID: " << base.msg_id() << std::endl;
                break;
        }
    }

    asio::io_context ioc_;
    std::unique_ptr<udp::socket> socket_;
    KcpSession::Ptr session_;
    uint32_t conv_;
    std::atomic<bool> running_{false};
    std::thread recv_thread_;
    std::thread loop_thread_;
    uint32_t target_msg_id_ = msg_id::Ping;
    uint32_t frame_count_ = 0;
};

int main(int argc, char* argv[]) {
    try {
        std::string ip = "127.0.0.1";
        uint16_t port = 8888;
        uint32_t conv = 1002;
        uint32_t msg_id = msg_id::Ping;

        // 参数解析策略：
        // 1. 如果有2个参数，且第2个是纯数字 -> msg_id
        // 2. 否则按 ip port msg_id 顺序解析
        if (argc == 2) {
            std::string arg1 = argv[1];
            if (arg1.find('.') != std::string::npos) {
                ip = arg1;
            } else {
                msg_id = std::stoul(arg1);
            }
        } else if (argc >= 3) {
            ip = argv[1];
            port = std::stoi(argv[2]);
            if (argc >= 4) {
                msg_id = std::stoul(argv[3]);
            }
        }

        std::cout << "Starting C++ Console Client connecting to " << ip << ":" << port 
                  << " with conv " << conv 
                  << " sending MsgID: " << msg_id << std::endl;
        
        ConsoleClient client(ip, port, conv, msg_id);
        client.start();
        
        // Block main thread
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }
    return 0;
}
