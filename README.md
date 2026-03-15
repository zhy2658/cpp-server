# KCP Server Framework

这是一个基于 C++17、ASIO、KCP、Protobuf 的高性能 UDP 服务器框架。本文档涵盖：
- 架构总览与运行时数据流
- 代码阅读顺序与关键文件
- 构建、运行、扩展新消息的步骤
- 常见问题与排错

## 架构与规范

详细的设计文档请参考 **[ARCHITECTURE.md](file:///d:/user/desktop/demo/cpp-server/ARCHITECTURE.md)**。

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

### 开发规范 (AI & Human)

为了保持代码质量，所有贡献者（包括 AI 辅助）必须遵守 [agents.md](file:///d:/user/desktop/demo/cpp-server/agents.md) 中的规范：
*   **无锁编程**：业务逻辑中禁止使用 `std::mutex`，必须通过 `asio::post` 投递到房间的 Strand 队列。
*   **内存安全**：禁止 `new/delete`，网络回调必须使用 `std::weak_ptr` 捕获会话。
*   **配置驱动**：所有参数必须通过 `config.yaml` 加载。

## 构建状态

项目已配置完成，依赖项 (KCP, ASIO, Protobuf, Abseil) 将通过 CMake `FetchContent` 自动下载并构建。
由于 Protobuf 需要从源码编译，**首次构建可能需要较长时间** (取决于机器性能，通常 5-15 分钟)。

## 依赖说明

*   **ASIO**: header-only，自动下载。
*   **KCP**: 源码集成，自动下载 (已 Patch 适配)。
*   **Protobuf**: 源码编译 (v21.12)，自动下载。
*   **spdlog / yaml-cpp**: 为简化构建流程，目前以标准 IO 和默认配置代替（可按需恢复）。

## 架构总览

- 网络 I/O：基于 ASIO 的 UDP 层，位于 [udp_server.h](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.h) / [udp_server.cpp](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.cpp)
- 会话抽象：每个 conv 一个 KCP 会话，负责分片/重传/流控，位于 [kcp_session.h](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.h) / [kcp_session.cpp](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.cpp)
- 会话管理：分片(shard) + 读写锁管理会话生命周期，位于 [session_manager.h](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.h) / [session_manager.cpp](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.cpp)
- 消息分发：解析 BaseMessage，按 msg_id 调用处理器，位于 [dispatcher.h](file:///d:/user/Deaktop/work/cpp-server/src/dispatcher.h)
- 协议模型：`proto/base.proto` → 生成 [base.pb.h](file:///d:/user/Deaktop/work/cpp-server/build/base.pb.h) / [base.pb.cc](file:///d:/user/Deaktop/work/cpp-server/build/base.pb.cc)
- 配置：默认配置结构体 [config.h](file:///d:/user/Deaktop/work/cpp-server/src/config.h)（支持 KCP 参数、线程数等）
- 工具：时间函数 `current_ms()` 位于 [utils.h](file:///d:/user/Deaktop/work/cpp-server/src/utils.h)

### 运行时数据流

```
UDP 收包 → UdpServer::do_receive
        → SessionManager::handle_input（按 conv 找/建会话，调用 KcpSession::input）
        → KCP 解包 → KcpSession::try_recv
        → Dispatcher::dispatch（查表转调用业务处理器）
        → 业务处理（如 on_ping）
        → KcpSession::send → UDP 发送

定时器（每 10ms）：
Timer → SessionManager::update_all（驱动 KCP）
      → SessionManager::check_timeout（会话超时淘汰）
```

### 并发模型
- 使用 `asio::io_context` 线程池，线程数由配置 `thread_pool_size` 控制。
- 同一 UDP socket 的发送通过 `asio::strand` 串行化，避免竞态。
- `Dispatcher::dispatch` 将业务处理投递到 `io_context`，由工作线程执行。
- `SessionManager` 采用按 conv 分片的哈希表，每片使用 `shared_mutex` 提升并发处理能力。

## 代码阅读顺序

1. 协议定义：[proto/base.proto](file:///d:/user/Deaktop/work/cpp-server/proto/base.proto)（BaseMessage、Ping/Pong）
2. 会话抽象：[kcp_session.h](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.h) → [kcp_session.cpp](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.cpp)
3. 会话管理：[session_manager.h](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.h) → [session_manager.cpp](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.cpp)
4. 分发器：[dispatcher.h](file:///d:/user/Deaktop/work/cpp-server/src/dispatcher.h)
5. UDP 服务器：[udp_server.h](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.h) → [udp_server.cpp](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.cpp)
6. 程序入口：[main.cpp](file:///d:/user/Deaktop/work/cpp-server/src/main.cpp)

阅读时重点关注：
- `KcpSession::input / send / try_recv / update / check` 的调用时机
- `SessionManager::handle_input` 如何在并发下安全地查找/创建会话
- `UdpServer` 的 `strand` 用法与定时驱动
- `Dispatcher` 的消息路由与注册接口

## 部署 (Docker)

推荐使用 Docker 进行生产环境部署，以解决依赖和环境一致性问题。

1.  **构建并启动**：
    ```bash
    docker-compose up -d --build
    ```
    
2.  **查看日志**：
    ```bash
    docker-compose logs -f
    ```

3.  **停止服务**：
    ```bash
    docker-compose down
    ```

更多部署细节请参考 [Dockerfile](file:///d:/user/desktop/demo/cpp-server/Dockerfile) 和 [docker-compose.yml](file:///d:/user/desktop/demo/cpp-server/docker-compose.yml)。

## 构建原理与过程详解 (Under the Hood)

当你运行 CMake 构建命令时，后台依次发生了以下过程：

### 1. 配置阶段 (Configuration)
CMake 读取根目录的 [CMakeLists.txt](file:///d:/user/Deaktop/work/cpp-server/CMakeLists.txt)，执行以下操作：
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
构建系统自动调用刚才生成的 `protoc.exe`，根据 [proto/base.proto](file:///d:/user/Deaktop/work/cpp-server/proto/base.proto) 生成 C++ 源文件：
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

## 构建步骤

1.  **生成构建文件** (已完成):
    ```bash
    # 注意：需要指定 MinGW 生成器和编译器路径 (根据你的环境)
    cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_PREFIX_PATH=D:/Devtool/protoc-34.0-win64 ...
    ```

2.  **编译**:
    在项目根目录运行：
    ```bash
    D:\Devtool\cmake\bin\cmake.exe --build build --config Release
    ```
    或者进入 `build` 目录运行 `mingw32-make`。

## 运行

构建成功后，可执行文件位于 `build/server.exe` (或 `build/server`)。

```bash
./build/server.exe
```

## 配置

- 默认端口: 8888
- 默认线程数: 4
- KCP 参数（如 nodelay/interval/sndwnd/rcvwnd/mtu）见 [config.h](file:///d:/user/Deaktop/work/cpp-server/src/config.h)
- 也可以在项目根放置 `config.yaml`（当前版本采用默认值；如需启用 YAML 解析，可按下文“可选依赖”恢复）

## 扩展：新增业务消息

示例：添加 `Login`（假设分配 `msg_id=1001`）
1. 在 [proto/base.proto](file:///d:/user/Deaktop/work/cpp-server/proto/base.proto) 添加消息体与 msg_id 约定。
2. 重新构建（会自动重新生成 `base.pb.*`）：
   ```bash
   D:\Devtool\cmake\bin\cmake.exe --build build --config Release -- -j4
   ```
3. 在合适位置实现处理函数（例如 `src/main.cpp` 外部或新文件）：
   ```cpp
   void on_login(KcpSession::Ptr s, const kcp_server::BaseMessage& base) {
       kcp_server::Login req; 
       if (!req.ParseFromString(base.payload())) return;
       // 处理并回包...
   }
   ```
4. 在程序启动时注册：
   ```cpp
   Dispatcher::instance().register_handler(1001, on_login);
   ```

## 可选依赖（按需开启）
- 日志：在 CMake 中恢复 `spdlog` 相关段，并把代码中的 `std::cout/cerr` 改回 `spdlog` 接口。
- 配置：在 CMake 中恢复 `yaml-cpp`，并在 [config.h](file:///d:/user/Deaktop/work/cpp-server/src/config.h) 的 `load` 中解析 `config.yaml`。

## 常见问题排查

- 首次编译时间很长  
  Protobuf 从源码构建，属于正常现象。可加 `-j4` 加速。

- MinGW GCC 8 与 `std::filesystem`  
  该版本对 `std::filesystem` 支持不完整，当前已移除依赖。如需使用，请升级 GCC（≥9）或在链接阶段加入 `-lstdc++fs`（不保证完全可用）。

- ZLIB 未找到  
  Protobuf 会提示 ZLIB 缺失，但当前未使用相关特性，可忽略；若需要请安装并在 CMake 中提供 `ZLIB_LIBRARY`/`ZLIB_INCLUDE_DIR`。

- 端口被占用/权限  
  启动时报地址绑定失败，请检查 8888 端口占用，或更换端口。

## 项目结构（摘录）

- CMake 与依赖：
  - [CMakeLists.txt](file:///d:/user/Deaktop/work/cpp-server/CMakeLists.txt)
  - `build/_deps/` 下为自动拉取与构建的第三方
- 源码：
  - [src/main.cpp](file:///d:/user/Deaktop/work/cpp-server/src/main.cpp)
  - [src/udp_server.h](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.h) / [src/udp_server.cpp](file:///d:/user/Deaktop/work/cpp-server/src/udp_server.cpp)
  - [src/kcp_session.h](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.h) / [src/kcp_session.cpp](file:///d:/user/Deaktop/work/cpp-server/src/kcp_session.cpp)
  - [src/session_manager.h](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.h) / [src/session_manager.cpp](file:///d:/user/Deaktop/work/cpp-server/src/session_manager.cpp)
  - [src/dispatcher.h](file:///d:/user/Deaktop/work/cpp-server/src/dispatcher.h)
  - [src/config.h](file:///d:/user/Deaktop/work/cpp-server/src/config.h), [src/utils.h](file:///d:/user/Deaktop/work/cpp-server/src/utils.h)
- 协议：
  - [proto/base.proto](file:///d:/user/Deaktop/work/cpp-server/proto/base.proto)

---

需要我把 `yaml-cpp` 与 `spdlog` 完整接回并提供一份生产推荐配置吗？我可以直接实现并更新文档。 
