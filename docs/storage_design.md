# 数据存储与持久化设计 (Storage Design)

## 1. 数据分层策略

| 数据类型 | 存储介质 | 读写频率 | 负责服务 | 示例 |
| :--- | :--- | :--- | :--- | :--- |
| **热数据 (Hot)** | **Memory (RAM)** | 极高 (60Hz) | C++ Server | 玩家实时坐标、血量、弹药 |
| **温数据 (Warm)** | **Redis** | 高 (RPC调用) | Golang/C++ | Session Token、房间列表、玩家在线状态 |
| **冷数据 (Cold)** | **MySQL/Mongo** | 低 (登录/结算) | Golang Server | 账号信息、背包道具、历史战绩 |

## 2. C++ Server 数据管理

C++ 战斗服原则上**不直接连接数据库 (MySQL)**，只连接 **Redis** (可选) 或通过 **RPC** 与 Golang 业务服交互。

### 2.1 玩家数据加载 (Load)
1.  玩家请求匹配。
2.  Golang 锁定玩家资产 (扣除门票)，将玩家基础属性 (Avatar, WeaponConfig) 打包。
3.  Golang -> C++ (`CreateRoom` / `JoinRoom` RPC): 传递玩家数据。
4.  C++ 将数据通过 `EnTT` 组件 (`Component`) 存入内存。

### 2.2 玩家数据持久化 (Save)
*   **实时保存**: 不推荐。战斗中不写库。
*   **结算保存 (Settlement)**:
    1.  战斗结束。
    2.  C++ 汇总战绩 (`Kills`, `Deaths`, `Damage`, `ItemsUsed`)。
    3.  C++ -> Golang (`ReportBattleResult` RPC): 发送战报。
    4.  Golang 校验战报合法性，写入 MySQL，更新排行榜。

### 2.3 RPC 协议格式 (gRPC / Internal TCP)
推荐使用 gRPC 进行服务间通信。

```protobuf
// battle_service.proto
service BattleService {
    // 战斗结束上报
    rpc ReportBattleResult(BattleResultRequest) returns (BattleResultResponse);
}

message BattleResultRequest {
    int64 room_id = 1;
    int64 end_time = 2;
    repeated PlayerResult players = 3;
}

message PlayerResult {
    int64 user_id = 1;
    int32 kills = 2;
    int32 deaths = 3;
    int32 damage_dealt = 4;
    map<int32, int32> items_used = 5; // item_id -> count
}
```

## 3. 断线重连 (Reconnection)

为了支持玩家掉线后快速重回战斗：

1.  **Session 保持**: 玩家掉线后，C++ 服务器保留其 `Entity` 和 `Session` 对象（标记为 `Disconnected`）一段时间（如 60秒）。
2.  **重连流程**:
    *   Client -> Golang (Login): 获取当前是否有未结束的战斗。
    *   Golang -> Redis: 查询 `user:{id}:session` 获取 `gate_ip` 和 `token`。
    *   Golang -> Client: 返回战斗服地址。
    *   Client -> C++ (UDP): 发送 `HandshakeReq { token, is_reconnect: true }`。
    *   C++: 校验 Token，恢复 `Session` 关联，下发全量 `WorldSnapshot`。

## 4. Redis Key 设计规范 (Draft)

*   `room:{id}:info` -> Hash { map_id, create_time, status }
*   `user:{id}:session` -> String { token, gate_ip, gate_port, room_id }
*   `rank:season:{id}` -> ZSet { score, user_id }

## 5. 灾备与回档
*   **崩溃恢复**: C++ 进程崩溃会导致房间销毁。需依靠 Golang 服检测心跳超时，退还玩家门票，并记录异常日志。
