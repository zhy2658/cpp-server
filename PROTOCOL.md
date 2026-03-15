# 已实现消息 Payload 说明

所有消息均以 `BaseMessage { msg_id, seq, payload }` 封装，`payload` 为业务消息的 Protobuf 序列化。

## MsgID 1 (Ping) — 客户端 → 服务器

客户端发送，非服务器返回：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| client_time | uint32 | 客户端发送时间戳 (ms) |

## MsgID 2 (Pong) — 服务器 → 客户端

收到 Ping 后服务器回复，用于心跳与 RTT 计算：

| 字段 | 类型 | 说明 |
| :--- | :--- | :--- |
| client_time | uint32 | 回传 Ping 的 client_time |
| server_recv | uint32 | 服务器接收 Ping 的时间 (ms) |
| server_send | uint32 | 服务器发送 Pong 的时间 (ms) |

**RTT 估算**：`server_send - client_time`。

## MsgID 2001 (WorldSnapshot) — 服务器 → 客户端

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
