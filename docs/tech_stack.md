# 技术栈选型规范 (Tech Stack & Conventions)

## 1. 黄金技术栈选型 (Standard Stack)

为了防止代码腐化，必须严格遵守以下选型：

| 领域 | 推荐库 | 理由 | 替代方案 (不推荐) |
| :--- | :--- | :--- | :--- |
| **ECS 框架** | **EnTT** | 现代 C++ 事实标准，性能极高，API 优雅 | 纯手写 Entity 类, Unity 风格的 GameObject |
| **物理引擎** | **Jolt Physics** | 下一代物理引擎，多线程性能强 (Horizon 使用) | PhysX (太重), Bullet3 (老旧) |
| **日志系统** | **spdlog** | 极快，异步，零成本抽象 | std::cout, printf, glog |
| **配置读取** | **yaml-cpp** | 适合人类阅读的配置文件格式 | XML, INI, 手写解析 |
| **数学库** | **glm** | 图形学标准，与 GLSL 一致，方便移植 Shader | Eigen (太重), 手写 Vector3 |
| **依赖管理** | **CMake FetchContent** | 原生支持，无需安装额外包管理器 | 手动下载 zip, git submodule |
| **AOI 算法** | **Grid / Quadtree** | 大地图视野管理标准解法 | 全广播 (O(N^2) 性能灾难) |

## 2. 目录结构规范

```text
src/
├── core/           # Layer 1: 日志, 数学, 配置 (如 config.h, utils.h)
├── network/        # Layer 2: 网络底层 (如 kcp_session, udp_server, dispatcher)
├── engine/         # Layer 3: 核心系统 (如 room, physics_world, ecs_registry)
├── gameplay/       # Layer 4: 玩法逻辑
│   ├── systems/    # 如 movement_system, combat_system
│   └── components/ # 如 transform, health
└── main.cpp        # 入口
```

## 3. 开发测试工具链

鉴于 Unity 测试流程繁琐（启动慢、编译久），**强烈建议**采用“工具先行”的开发模式。

**工作流： cpp-server (后端) + KcpProbe (调试终端/协议验证) + Unity (最终表现)**

1.  **无缝移植**：`KcpProbe` 中的 `Kcp.Core` 模块是纯 .NET Standard 代码，可直接复制到 Unity 项目中复用。
2.  **验证闭环**：开发新协议时，先在 `KcpProbe` (WinUI) 中添加测试按钮或页签。验证逻辑正确后，再去 Unity 接入表现。这能提升 10 倍开发效率。
3.  **自动化回归**：利用 `Kcp.SmokeTests`，每次修改 C++ 核心代码后运行测试，确保服务器稳定。
