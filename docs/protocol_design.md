# 协议与消息定义规范 (Protocol Design)

## 1. 协议设计原则

*   **序列化格式**: Protobuf v3 (二进制流，紧凑高效)。
*   **传输层**: UDP + KCP (核心战斗), TCP/HTTP (业务逻辑)。
*   **大小端**: 网络字节序 (Big Endian)。
*   **消息结构**:
    ```
    [4 bytes KCP Conv] [KCP Header...] [Protobuf Payload]
    ```

## 2. 消息 ID 分段 (Message ID Allocation)

为了避免 ID 冲突，严格按照功能模块划分 MsgID：

| ID 范围 | 模块 | 描述 |
| :--- | :--- | :--- |
| **0 - 99** | **System** | 心跳, 握手, 错误码, 时间同步 |
| **100 - 999** | **Lobby** | 登录, 匹配, 房间列表, 聊天 |
| **1000 - 1999** | **Room** | 进出房间, 加载地图, 准备就绪 |
| **2000 - 2999** | **Battle (Sync)** | 移动同步, 动作状态, 武器切换 |
| **3000 - 3999** | **Battle (Event)** | 射击, 命中, 死亡, 复活, 掉落拾取 |
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
*   **Event**: 关键事件（开火、击杀）实时可靠发送。

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
