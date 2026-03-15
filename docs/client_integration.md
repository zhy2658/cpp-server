# 客户端接入指南 (Client Integration Guide)

## 1. 概述
本指南面向 Unity/Unreal 客户端开发者，说明如何接入 `cpp-server` 战斗服。
核心难点在于：**预测 (Prediction)** 、 **和解 (Reconciliation)** 与 **插值 (Interpolation)**。

## 2. 接入流程

### 2.1 网络层接入
1.  引入 `Kcp.Core` (C#) 或相应 KCP 库。
2.  实现 `IConnection` 接口，对接 Protobuf 协议。
3.  **时钟同步 (Clock Sync)**:
    *   客户端需维护 `ServerTime`，目标是将客户端时间与服务器时间对齐。
    *   **算法 (NTP-like)**:
        1.  Client 发送 `Ping { t1: LocalTime }`。
        2.  Server 回复 `Pong { t1, t2: ServerRecvTime, t3: ServerSendTime }`。
        3.  Client 收到 `Pong` (t4)，计算 `RTT = (t4 - t1) - (t3 - t2)`。
        4.  `Offset = (t2 - t1 + t3 - t4) / 2`。
        5.  `ServerTime = LocalTime + Offset`。
    *   建议每秒同步一次，取最近 5 次采样的中位数以消除抖动。

### 2.2 移动同步 (Movement Sync)

#### 本地玩家：客户端预测 (Client-side Prediction)
*   **不要等待服务器！**
*   当玩家按下 `W` 键：
    1.  立即在本地位移玩家。
    2.  记录输入指令 `Input { seq=100, dir=(0,0,1), dt=0.016 }` 到历史队列。
    3.  发送 `Input` 给服务器 (UDP Unreliable)。

#### 本地玩家：服务器和解 (Server Reconciliation)
*   客户端收到服务器快照 `Snapshot { last_processed_seq=100, pos=(10,0,10) }`。
*   **校验**: 对比本地历史队列中 `seq=100` 时的位置与服务器位置。
*   **回滚**:
    *   如果误差 < 阈值 (如 0.1m): 忽略，或进行极微小的平滑修正。
    *   如果误差 > 阈值 (如被服务器拉回): **强制设置位置为服务器位置**，并**重放 (Replay)** `seq=101` 及其后的所有输入，以追赶到当前帧。

#### 远端玩家：实体插值 (Entity Interpolation)
*   对于其他玩家 (Remote Players)，客户端收到的位置是“过去”的（因为网络延迟）。
*   **不要直接设置坐标**，否则会导致瞬移和卡顿。
*   **插值算法**:
    1.  客户端维护一个 `SnapshotBuffer`，存储最近收到的快照。
    2.  设定 `RenderDelay` (如 100ms)，使得 `RenderTime = ServerTime - RenderDelay`。
    3.  在 Buffer 中找到两个快照 `S_prev` 和 `S_next`，满足 `S_prev.time <= RenderTime <= S_next.time`。
    4.  计算插值比例 `alpha = (RenderTime - S_prev.time) / (S_next.time - S_prev.time)`。
    5.  `CurrentPos = Lerp(S_prev.pos, S_next.pos, alpha)`。

### 2.3 射击与命中
*   **客户端**: 播放枪口火光、音效（立即反馈）。
*   **服务器**: 权威判定命中。
*   **命中反馈**:
    *   客户端显示“击中提示 (Crosshair Hitmarker)”必须等待服务器的 `HitEvent` 下发。
    *   **不要在客户端直接扣血**。

## 3. 调试工具
*   使用 `KcpProbe` 工具模拟高延迟 (Simulate Latency) 和丢包 (Packet Loss)，验证预测算法的稳定性。
*   开启 `DrawDebug` 接收服务器的碰撞盒线框，对比客户端位置。
