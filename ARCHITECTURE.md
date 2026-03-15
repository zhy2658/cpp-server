# cpp-server 项目架构与设计规范 (Architecture & Design)

本文档是 `cpp-server` 战斗服务器的入口导航页。所有的架构设计、开发规范和技术细节均已模块化拆分。请根据需要查阅以下子系统文档。

## 1. 核心架构与分层 (Core Architecture)

*   **[系统分层与微服务职责 (System Layering & Microservices)](docs/system_architecture.md)**
    *   描述 C++ 战斗服与 Golang 业务服的职责划分，以及 C++ 内部的 Core/Network/Engine/Gameplay 四层架构和依赖流向。
*   **[技术栈选型规范 (Tech Stack & Conventions)](docs/tech_stack.md)**
    *   详细列出 ASIO, KCP, EnTT, Jolt 等核心库的选型理由，以及项目目录结构和开发测试工具链推荐。
*   **[并发与内存管理模型 (Concurrency & Memory)](docs/concurrency_model.md)**
    *   阐述无锁网络模型、基于 Strand 的单线程序列化逻辑队列、智能指针防悬空原则及 ECS 实体管理规范。

## 2. 核心战斗逻辑 (Battle Core)

*   **[游戏循环与房间生命周期 (Game Loop & Room Lifecycle)](docs/game_loop.md)**
    *   定义房间的状态机转换，以及核心 60Hz Tick 的物理模拟、ECS 更新、网络广播执行顺序。
*   **[伤害计算与延迟补偿 (Damage & Lag Compensation)](docs/damage_calculation.md)**
    *   解析 CS2 同款 Sub-tick 架构原理、历史快照回滚机制及射线/碰撞检测的权威判定流程。
*   **[协议与消息定义规范 (Protocol Design)](docs/protocol_design.md)**
    *   定义 Protobuf 结构、MsgID 严格分段、客户端登录握手交互流程及系统错误码标准。

## 3. 安全与数据 (Security & Data)

*   **[服务器校验与防篡改机制 (Server Validation)](docs/server_validation.md)**
    *   阐述“永不信任客户端”的四层防作弊体系，包括移动速度校验、视线遮挡检测、逻辑混淆与签名机制。
*   **[数据存储与持久化设计 (Storage Design)](docs/storage_design.md)**
    *   定义内存热数据、Redis 温数据与 MySQL 冷数据的分层处理，以及战斗结算的数据库落地策略。

## 4. 工程化与接入 (Engineering & Integration)

*   **[客户端接入指南 (Client Integration Guide)](docs/client_integration.md)**
    *   面向 Unity/UE 等客户端开发者，说明如何实现客户端先行预测 (Prediction) 与服务器强制和解 (Reconciliation)。
*   **[运维与监控标准 (Ops & Monitoring)](docs/ops_guide.md)**
    *   包含 Docker 容器化部署指南、Prometheus 关键性能指标 (如 Tick 耗时) 定义及 spdlog 结构化日志规范。