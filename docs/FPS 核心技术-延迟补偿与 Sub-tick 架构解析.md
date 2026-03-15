# FPS 核心技术：延迟补偿与 Sub-tick 架构解析

本文档详细解析了 3D PVP 射击游戏中的核心同步技术，包括延迟补偿 (Lag Compensation)、CS2 的 Sub-tick 架构以及服务器性能优化策略。

## 1. 延迟补偿 (Lag Compensation) 流程

在网络延迟存在的情况下，如何保证“所见即所得”的射击体验？核心机制是服务器的**时间回溯**。

### 1.1 完整交互时序图

```mermaid
sequenceDiagram
    participant A as 客户端 A (攻击者)
    participant S as 服务器 (权威)
    participant B as 客户端 B (受害者)
    participant O as 其他客户端

    Note over A: 玩家按下开火键 (t=1000ms)
    A->>A: 立即播放枪声/火光 (客户端预测)
    A->>S: 发送指令 {Action:FIRE, Time:1000ms, Dir:...}
    
    Note over A,S: 网络传输 (延迟 50ms)
    
    Note over S: 服务器在 t=1050ms 收到包
    S->>S: 1. 读取指令时间戳 (1000ms)
    S->>S: 2. 回溯 (Rollback): 将所有玩家移回 1000ms 时的位置
    S->>S: 3. 射线检测 (Raycast): 判定是否命中
    S->>S: 4. 恢复现场: 将玩家移回 1050ms 位置
    
    alt 判定命中
        S->>S: 扣除 B 血量
        S->>A: 广播 {Event:HIT, Target:B, Damage:100}
        S->>B: 广播 {Event:HIT, Target:B, Damage:100}
        S->>O: 广播 {Event:HIT, Target:B, Damage:100}
    end

    B->>B: 播放受击动画/死亡
    A->>A: 显示击杀图标
```

### 1.2 处理流程图

```mermaid
flowchart TD
    A[玩家A按下攻击键] --> B{客户端A}
    B --> C[播放攻击动画<br>（客户端预测）]
    B --> D[向服务器发送<br>“攻击请求”数据包]

    D --> E[服务器<br>（权威计算中心）]
    
    subgraph E [服务器核心计算]
        E1[校验攻击合法性<br>（冷却/距离/状态）]
        E2[命中判定<br>（基于服务器位置/延迟补偿）]
        E3[伤害计算<br>（攻击力/防御/暴击）]
        E4[更新玩家B血量]
    end
    
    E4 --> F[向全服广播<br>“玩家B受伤害”事件]
    
    F --> G[客户端A]
    F --> H[客户端B]
    F --> I[其他客户端]
    
    G --> G1[播放受击/扣血特效]
    H --> H1[自身受击表现<br>显示伤害数字]
    I --> I1[同步看到B掉血]
```

---

## 2. CS2 Sub-tick 架构解析

CS2 引入的 Sub-tick 技术是对传统 Tick-based 架构的重大革命，旨在消除 Tickrate (64/128) 带来的手感差异。

### 2.1 传统 vs Sub-tick

| 特性 | 传统 FPS (CS:GO) | CS2 (Sub-tick) |
| :--- | :--- | :--- |
| **输入精度** | 只能精确到 Tick (每 15.6ms) | 精确到**微秒级**时间戳 |
| **服务器视角** | "你在第 10 帧开了一枪" | "你在第 10 帧 + **3.14ms** 开了一枪" |
| **回溯逻辑** | 回溯到第 10 帧的整点位置 | 回溯到第 10 帧位置 + **3.14ms 的插值位移** |
| **射击体验** | 受 Tickrate 限制，可能有微小偏差 | **无限 Tick** 的判定精度 |

### 2.2 实现关键点
1.  **高精度协议**：客户端上报的包必须包含 `Ratio` (帧内偏移量) 或 `Microsecond Timestamp`。
2.  **连续物理模拟**：服务器必须支持在两个物理帧之间进行插值 (Interpolation)，构建出“不存在于任何 Tick 上”的中间状态。

---

## 3. 服务器性能优化策略

如何在保证权威计算（防作弊）的同时，承载大量玩家？需要宏观与微观的结合。

```mermaid
flowchart LR
    subgraph A[宏观优化：减少计算量]
        direction LR
        A1[区域管理与<br>AOI<br>（只关心附近玩家）]
        A2[网格/空间分割<br>（只检测相邻格子）]
    end

    subgraph B[微观优化：降低精度与频率]
        direction LR
        B1[粗检测与细检测<br>（先包围盒后网格）]
        B2[计算频率降级<br>（不同距离不同更新率）]
    end

    subgraph C[软硬件结合：提升算力]
        direction LR
        C1[多线程/ECS架构<br>（并行计算）]
        C2[分布式/负载均衡<br>（分区分服）]
    end

    A & B & C --> D[结论：服务器在<br>“可接受的成本”下<br>保证了公平性]
```

### 3.1 核心优化手段
*   **AOI (Area of Interest)**: 使用九宫格或四叉树算法，只对玩家视野内的实体进行同步和物理检测。
*   **LOD (Level of Detail)**: 远处的物体降低物理检测频率（如每秒只算 10 次），近处的物体全频率计算（每秒 60 次）。
*   **ECS 并行化**: 利用 EnTT 等框架，将无依赖的系统（如移动、回血）分散到多核 CPU 上并行执行。
