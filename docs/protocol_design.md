# 协议与消息定义规范 (Protocol Design)

## 1. 协议设计原则

*   **序列化格式**: Protobuf v3 (二进制流，紧凑高效)。
*   **传输层**: UDP + KCP (核心战斗), TCP/HTTP (业务逻辑)。
*   **大小端**: 网络字节序 (Big Endian)。
*   **消息结构**:
    ```
    [4 bytes KCP Conv] [KCP Header...] [Protobuf Payload]
    ```

### 1.1 KCP 通道划分 (Channels)
为了平衡实时性与可靠性，使用不同的 KCP 通道：

*   **Channel 0 (Unreliable)**: 移动同步 (`Input`, `Snapshot`)。丢包不重传，永远只处理最新的包。
*   **Channel 1 (Reliable)**: 关键逻辑 (`Fire`, `Hit`, `Die`, `JoinRoom`)。必须到达，且有序。

### 1.2 MTU 与分包 (Fragmentation)
*   **MTU 限制**: 默认 `1200` 字节 (保守值，适应公网环境)。
*   **分包策略**:
    *   KCP 协议层自动处理分包 (Fragment)。
    *   **应用层限制**: 单个 Protobuf 包体尽量控制在 1KB 以内。
    *   **大包处理**: 如需发送地图数据 (>10KB)，应在应用层拆分为多个 Chunk (`MapDataChunk { index, total, data }`)，避免阻塞 KCP 发送队列。

## 2. 消息 ID 分段 (Message ID Allocation)

为了避免 ID 冲突，严格按照功能模块划分 MsgID：

| ID 范围 | 模块 | 描述 |
| :--- | :--- | :--- |
| **0 - 99** | **System** | 心跳, 握手, 错误码, 时间同步 |
| **100 - 999** | **Lobby** | 登录, 匹配, 房间列表, 聊天 |
| **1000 - 1999** | **Room** | 进出房间, 加载地图, 准备就绪 |
| **2000 - 2999** | **Battle (Sync)** | 移动同步, 动作状态, 武器切换 |
| **3000 - 3999** | **Battle (Event)** | 射击, 近战攻击, 命中, 死亡, 复活, 掉落拾取 |
| **4000 - 4999** | **Debug** | GM 指令, 调试信息 |

## 3. 核心交互流程 (Interaction Flow)

### 3.1 登录与握手 (Handshake)
1.  **Client -> Server (HTTP)**: 登录账号，获取 `SessionToken` 和 `KcpPort`。
2.  **Client -> Server (UDP)**: 发送 `HandshakeReq { token }`。
3.  **Server**: 验证 Token，分配 `KcpConv`。
4.  **Server -> Client (UDP)**: 返回 `HandshakeRsp { conv, seed }`。

### 3.2 战斗循环 (Battle Loop)
*   **Input**: 客户端以 60Hz/128Hz 采样输入，发送 `PlayerInput { move_dir, view_angle, buttons, sub_tick_time }`。
*   **Snapshot**: 服务器以 20Hz/30Hz 广播 `WorldSnapshot { entities: [ {id, pos, vel, anim_state} ] }`。
*   **Event**: 关键事件（开火、近战攻击、击杀）实时可靠发送。

### 3.3 已实现消息 Payload 说明

所有消息均以 `BaseMessage { msg_id, seq, payload }` 封装，`payload` 为业务消息的 Protobuf 序列化。

#### MsgID 1 (Ping) — 客户端 → 服务器

客户端发送，非服务器返回：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| client_time | uint32 | 客户端发送时间戳 (ms) |

#### MsgID 2 (Pong) — 服务器 → 客户端

收到 Ping 后服务器回复，用于心跳与 RTT 计算：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| client_time | uint32 | 回传 Ping 的 client_time |
| server_recv | uint32 | 服务器接收 Ping 的时间 (ms) |
| server_send | uint32 | 服务器发送 Pong 的时间 (ms) |

**RTT 估算**：`server_send - client_time`。

#### MsgID 2001 (WorldSnapshot) — 服务器 → 客户端

服务器以 20～30Hz 广播的世界快照：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| seq | uint32 | 快照序列号，每帧递增 |
| server_time | uint32 | 服务器时间戳 (ms) |
| entities | repeated EntityState | 实体列表 |

**EntityState 单条结构**：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| entity_id | uint32 | 实体 ID |
| pos | Vector3 | 位置 (x, y, z) |
| vel | Vector3 | 速度 (x, y, z) |
| yaw | float | 水平朝向角度 |
| hp | int32 | 血量 |
| state_flags | uint32 | 状态位掩码（蹲下、跳跃、死亡等） |

当前实现中，每个连接对应一个占位实体，`entity_id` 为 KCP conv，`pos` 默认 (0,0,0)，`hp` 默认 100。

### 3.4 Battle Event 消息与 MsgID

| MsgID | 消息 | 方向 | 说明 |
| :--- | :--- | :--- | :--- |
| **3001** | FireEvent | C→S | 远程武器开火，采用 Raycast 判定 |
| **3002** | MeleeAttackEvent | C→S | 近战攻击，采用 Sphere/Box Overlap 判定 |
| **3010** | HitEvent | S→C | 命中广播（远程/近战统一） |

**FireEvent** 字段说明：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| shooter_id | uint32 | 开火者实体 ID |
| origin | Vector3 | 射线起点（枪口位置） |
| direction | Vector3 | 射线方向（单位向量） |
| weapon_id | uint32 | 武器配置 ID |
| weapon_type | uint32 | 0=Ranged, 1=Melee（兼容复用） |

**MeleeAttackEvent** 字段说明：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| attacker_id | uint32 | 攻击者实体 ID |
| direction | Vector3 | 攻击方向（单位向量） |
| weapon_id | uint32 | 武器配置 ID |
| timestamp | uint32 | 客户端时间戳 (ms)，用于延迟补偿 |

### 3.5 Protobuf 消息定义

最新的消息结构定义请直接参考项目源码：
*   **[proto/base.proto](../../proto/base.proto)**

该文件包含了 System, Battle Sync, Battle Event 等所有核心协议的 Protobuf 定义，是开发的唯一真理来源 (Single Source of Truth)。

## 4. 错误码定义 (Error Codes)

| Code | Name | Description |
| :--- | :--- | :--- |
| 0 | OK | 成功 |
| 1001 | ERR_TOKEN_INVALID | Token 无效或过期 |
| 2001 | ERR_ROOM_NOT_FOUND | 房间不存在 |
| 2002 | ERR_ROOM_FULL | 房间已满 |
| 3001 | ERR_SYNC_ILLEGAL | 移动速度异常 (SpeedHack) |

## 5. Proto 文件管理
所有 `.proto` 文件存放于 `proto/` 目录，禁止修改自动生成的代码。
