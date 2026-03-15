# 客户端接入指南 (Client Integration Guide)

## 1. 概述
本指南面向 Unity/Unreal 客户端开发者，说明如何接入 `cpp-server` 战斗服。
核心难点在于：**预测 (Prediction)** 与 **和解 (Reconciliation)**。

## 2. 接入流程

### 2.1 网络层接入
1.  引入 `Kcp.Core` (C#) 或相应 KCP 库。
2.  实现 `IConnection` 接口，对接 Protobuf 协议。
3.  **时钟同步**:
    *   客户端需维护 `ServerTime`。
    *   每秒发送 `Ping`，根据 `RTT` 计算 `ServerTime = LocalTime + Offset`。

### 2.2 移动同步 (Movement Sync)

#### 客户端预测 (Client-side Prediction)
*   **不要等待服务器！**
*   当玩家按下 `W` 键：
    1.  立即在本地位移玩家。
    2.  记录输入指令 `Input { seq=100, dir=(0,0,1), dt=0.016 }` 到历史队列。
    3.  发送 `Input` 给服务器。

#### 服务器和解 (Server Reconciliation)
*   客户端收到服务器快照 `Snapshot { last_processed_seq=100, pos=(10,0,10) }`。
*   **校验**: 对比本地历史队列中 `seq=100` 时的位置与服务器位置。
*   **回滚**:
    *   如果误差 < 阈值 (如 0.1m): 平滑插值修正。
    *   如果误差 > 阈值 (如被服务器拉回): **强制设置位置为服务器位置**，并**重放 (Replay)** `seq=101` 及其后的所有输入，以追赶到当前帧。

### 2.3 射击与命中
*   **客户端**: 播放枪口火光、音效（立即反馈）。
*   **服务器**: 权威判定命中。
*   **命中反馈**:
    *   客户端显示“击中提示 (Crosshair Hitmarker)”必须等待服务器的 `HitEvent` 下发。
    *   **不要在客户端直接扣血**。

## 3. 调试工具
*   使用 `KcpProbe` 工具模拟高延迟 (Simulate Latency) 和丢包 (Packet Loss)，验证预测算法的稳定性。
*   开启 `DrawDebug` 接收服务器的碰撞盒线框，对比客户端位置。
