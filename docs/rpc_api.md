# C++ 与 Golang 服务间 RPC 接口规范 (RPC API)

本文档定义 `cpp-server` 与 `go-server` 之间的 gRPC 接口，用于匹配、房间管理与战斗结算。

## 1. 通信方式

*   **协议**：gRPC (HTTP/2 + Protobuf)
*   **方向**：Golang 为客户端，C++ 为服务端（Golang 主动调用 C++）
*   **备选**：Redis Pub/Sub 用于轻量级通知

## 2. 服务定义 (battle_service.proto)

```protobuf
service BattleService {
    // 创建房间（匹配成功后调用）
    rpc CreateRoom(CreateRoomRequest) returns (CreateRoomResponse);
    // 玩家加入房间
    rpc JoinRoom(JoinRoomRequest) returns (JoinRoomResponse);
    // 玩家离开/掉线通知
    rpc LeaveRoom(LeaveRoomRequest) returns (LeaveRoomResponse);
    // 战斗结算上报
    rpc ReportBattleResult(BattleResultRequest) returns (BattleResultResponse);
    // 心跳/存活检测（可选）
    rpc HealthCheck(HealthCheckRequest) returns (HealthCheckResponse);
}

message CreateRoomRequest {
    int64 match_id = 1;
    int32 map_id = 2;
    repeated PlayerSlot players = 3;
}

message PlayerSlot {
    int64 user_id = 1;
    string token = 2;
    bytes avatar_config = 3;  // 角色/武器配置
}

message CreateRoomResponse {
    int32 code = 1;
    int64 room_id = 2;
    string error_message = 3;
}

message JoinRoomRequest {
    int64 room_id = 1;
    int64 user_id = 2;
    string token = 3;
}

message JoinRoomResponse {
    int32 code = 1;
    string error_message = 2;
}

message LeaveRoomRequest {
    int64 room_id = 1;
    int64 user_id = 2;
    int32 reason = 3;  // 0=主动, 1=掉线, 2=踢出
}

message LeaveRoomResponse {
    int32 code = 1;
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
    map<int32, int32> items_used = 5;
}

message BattleResultResponse {
    int32 code = 1;
    string error_message = 2;
}
```

## 3. 调用方职责

| 接口 | 调用方 | 说明 |
| :--- | :--- | :--- |
| CreateRoom | Golang | 匹配成功时，传入玩家列表，C++ 分配 room_id 并初始化 |
| JoinRoom | Golang | 通知 C++ 某玩家将连入指定房间，C++ 预留 Slot |
| LeaveRoom | 双方 | Golang 主动踢人时调用；C++ 检测掉线后也可回调 Golang |
| ReportBattleResult | C++ | 战斗结束时 C++ 主动调用 Golang，Golang 落库并更新排行榜 |
