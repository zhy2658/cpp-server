# KCP Server Framework

这是一个基于 C++17、ASIO、KCP、Protobuf 的高性能 UDP 服务器框架。本文档涵盖：
- 架构设计与规范
- 核心系统层级与数据流
- 依赖管理与构建原理

## 架构与规范

详细的设计文档请参考 **[ARCHITECTURE.md](./ARCHITECTURE.md)**。

### 核心架构 (Four-Layer Architecture)

本项目采用严格的四层分层架构，确保职责单一，防止腐化。

| 层级 | 名称 | 职责 | 依赖关系 |
| :--- | :--- | :--- | :--- |
| **L4** | **Gameplay** | 纯粹的玩法逻辑 (ECS Systems)。例如：扣血、移动、技能释放。 | 依赖 Engine |
| **L3** | **Engine** | 游戏引擎核心。负责对象生命周期、房间管理、物理世界 (Jolt)、ECS 注册表 (EnTT)。 | 依赖 Network |
| **L2** | **Network** | 网络底层。负责 UDP 收发、KCP 协议处理、消息分发。**严禁包含玩法逻辑**。 | 依赖 Core |
| **L1** | **Core** | 基础设施。日志 (spdlog)、配置 (yaml-cpp)、数学库 (glm)。 | 无依赖 |

### 关键技术选型

| 领域 | 选型 | 理由 |
| :--- | :--- | :--- |
| **语言** | **C++17** | 兼顾现代特性与稳定性。 |
| **ECS** | **EnTT** | 业界性能最强的 C++ ECS 框架，优于 OOP 继承。 |
| **物理** | **Jolt Physics** | 多线程性能极佳的次世代物理引擎 (Horizon 使用)。 |
| **并发** | **ASIO + Strand** | 使用 Strand 实现无锁并发，每个房间单线程处理，避免 Mutex 死锁。 |
| **视野** | **Grid / Quadtree** | 使用高效的 AOI 算法管理大地图视野，拒绝 O(N^2) 全量广播。 |
| **同步** | **Sub-tick** | 采用 CS2 同款 Sub-tick 架构，支持微秒级输入回溯，实现无限 Tick 射击精度。 |
| **优化** | **LOD / BroadPhase** | 采用粗细检测分离与频率降级策略，大幅降低物理计算开销。 |

### 开发规范 (AI & Human)

为了保持代码质量，所有贡献者（包括 AI 辅助）必须遵守 [agents.md](./agents.md) 中的规范：
*   **无锁编程**：业务逻辑中禁止使用 `std::mutex`，必须通过 `asio::post` 投递到房间的 Strand 队列。
*   **内存安全**：禁止 `new/delete`，网络回调必须使用 `std::weak_ptr` 捕获会话。
*   **配置驱动**：所有参数必须通过 `config.yaml` 加载。

## 依赖说明

*   **ASIO**: header-only，自动下载。
*   **KCP**: 源码集成，自动下载 (已 Patch 适配)。
*   **Protobuf**: 源码编译 (v21.12)，自动下载。
*   **spdlog / yaml-cpp**: 为简化构建流程，目前以标准 IO 和默认配置代替。

## 系统架构详解

### 1. 静态层级：谁拥有谁？ (Ownership Hierarchy)

整个服务器架构类似一个**快递转运中心**：

*   **[GameServer](src/server/game_server.h) (CEO)**
    *   **持有**：`asio::io_context` (整个公司的**劳动力池/线程池**)。
    *   **持有**：`UdpServer` (**前台大厅**，负责对外营业)。
    *   **职责**：读取配置，初始化环境，启动主循环。

*   **[UdpServer](src/network/udp_server.h) (前台大厅)**
    *   **持有**：`socket` (大门，唯一的 UDP 端口)。
    *   **持有**：`SessionManager` (**客户档案部**)。
    *   **持有**：两个核心线程
        *   `recv_thread`：专门负责收件（`socket.receive_from`）。
        *   `update_thread`：专门负责打卡（驱动 KCP 的 `update` 心跳）。
    *   **职责**：将收到的原始 UDP 包转交给 SessionManager，并驱动时间流逝。

*   **[SessionManager](src/network/session_manager.h) (客户档案部)**
    *   **持有**：`Shards` (**分片桶**)。为了防止锁冲突，将会话按 ID 拆分到 32 个桶中，每个桶有一把锁 (`std::mutex`)。
    *   **职责**：
        *   根据 ID (`conv`) 查找对应的 `KcpSession`。
        *   管理会话生命周期（创建新会话、超时剔除）。

*   **[KcpSession](src/network/kcp_session.h) (专属客服)**
    *   **持有**：`ikcpcb` (KCP 协议核心结构体)。
    *   **职责**：
        *   **翻译**：处理 KCP 协议细节（重传、乱序重排、流控）。
        *   **剥壳**：从 KCP 数据流中还原出完整的应用层数据包。

*   **[Dispatcher](src/network/dispatcher.h) (分拣中心 - 单例)**
    *   **持有**：`Map<msg_id, Handler>` (分发路由表)。
    *   **职责**：根据 Protobuf 消息中的 `msg_id` 将请求分发给对应的 Handler。

### 2. 动态流向：一个 Ping 包的奇幻漂流

以 **Ping 请求 (ID: 1)** 为例，数据从网络到底层再返回的完整流程：

#### 阶段一：接收与协议处理 (IO 线程)
1.  **UDP 接收**：`UdpServer` 的 `recv_thread` 收到原始二进制数据。
2.  **查找会话**：`SessionManager` 根据包头 ID 在分片桶中找到对应的 `KcpSession`。
3.  **KCP 注入**：调用 `KcpSession::input()`，将数据喂给 KCP 协议栈。
4.  **应用层提取**：调用 `KcpSession::try_recv()`。如果数据包完整，KCP 吐出应用层 Protobuf 二进制包。

#### 阶段二：逻辑分发 (IO 线程 -> 逻辑线程)
5.  **回调触发**：`SessionManager` 调用数据回调。
6.  **路由分发**：`Dispatcher::dispatch()` 被调用。
    *   解析 Protobuf 头部，识别 `msg_id = 1`。
    *   查找注册的 `handlers::on_ping`。
7.  **关键切换**：`Dispatcher` 使用 `asio::post(ioc, ...)`。
    *   **关键点**：任务被封装并投递到 `io_context` 线程池，**立刻释放 IO 线程**。

#### 阶段三：业务执行 (Worker 线程)
8.  **执行逻辑**：线程池中的 Worker 线程取出任务，执行 `handlers::on_ping`。
    *   解析 Ping，计算时间。
    *   生成 Pong 响应。
9.  **发送响应**：调用 `session->send(Pong数据)`。
    *   数据被压入 `KcpSession` 的发送队列。

#### 阶段四：发送 (后台驱动)
10. **KCP 打包**：`UdpServer` 的 `update_thread` 定时（10ms）调用 `KcpSession::update()`。
11. **实际发送**：KCP 决定发送数据（或 ACK），调用底层 `output` 回调。
12. **UDP 发出**：数据通过 `socket` 发回客户端。

```text
[UDP Socket]
     ⬇️ (Raw Bytes)
[UdpServer (recv_thread)]
     ⬇️
[SessionManager] --> 查找/创建 Session
     ⬇️
[KcpSession] --> ikcp_input (处理重传/排序)
     ⬇️ (完整业务包)
[Dispatcher]
     ⬇️ asio::post (切线程) 🔀
----------------------------------------
[Worker Thread]
     ⬇️
[Handler (on_ping)] --> 业务逻辑
     ⬇️ session->send()
[KcpSession] --> 放入发送队列
     ⬇️ (等待 update_thread 驱动)
[UdpServer (socket)] --> 发送回客户端
```

## 构建原理与过程详解 (Under the Hood)

当你运行 CMake 构建命令时，后台依次发生了以下过程：

### 1. 配置阶段 (Configuration)
CMake 读取根目录的 [CMakeLists.txt](./CMakeLists.txt)，执行以下操作：
- **检测环境**：识别编译器 (GCC/MinGW) 和构建工具。
- **依赖管理 (FetchContent)**：
  - 自动从 GitHub 下载 **KCP**, **ASIO**, **Protobuf**, **Abseil** 的源码压缩包。
  - 解压并配置这些第三方库到 `build/_deps` 目录。
  - 针对 KCP 这种旧版 CMake 项目，自动应用 Patch 修复版本兼容性。
- **生成构建文件**：根据你的选择 (MinGW Makefiles)，生成 `Makefile` 和相关辅助文件。

### 2. 依赖构建 (Dependency Build)
在正式编译你的代码前，构建系统会先编译第三方库：
- **Abseil**: Google 的 C++ 基础库，是 Protobuf 的依赖项。
- **Protobuf (耗时最长)**：
  - **protoc**: 首先编译出 Protobuf 编译器可执行文件 (`protoc.exe`)。
  - **libprotobuf**: 编译 Protobuf 运行时库 (`libprotobuf.a`)，供服务器链接使用。
- **KCP**: 编译 KCP 核心逻辑为静态库 (`libkcp.a`)。

### 3. 代码生成 (Code Generation)
构建系统自动调用刚才生成的 `protoc.exe`，根据 [proto/base.proto](./proto/base.proto) 生成 C++ 源文件：
- 生成 `build/base.pb.h` (头文件)
- 生成 `build/base.pb.cc` (源文件)
这些文件包含了消息序列化/反序列化的具体实现。

### 4. 项目编译 (Compilation)
编译器 (g++) 开始工作：
- 编译生成的 `base.pb.cc`。
- 编译项目源码：`src/main.cpp`, `src/udp_server.cpp`, `src/kcp_session.cpp` 等。
- 生成对应的 `.obj` 目标文件。

### 5. 链接 (Linking)
链接器 (ld) 将所有部分组装在一起：
- 你的 `.obj` 文件
- `base.pb.obj`
- 静态库：`libkcp.a`, `libprotobuf.a`, `libabseil*.a`
- 系统库：`ws2_32` (Windows Socket 库)

最终生成可执行文件 `build/server.exe`。
