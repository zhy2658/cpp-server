# 服务器校验与防篡改机制 (Server Validation & Anti-Cheat)

本文档详细阐述了 PVP 游戏服务器如何防止客户端作弊和消息篡改。

## 1. 核心原则：永不信任客户端

**所有的防御机制都基于一个核心假设：客户端运行在用户的机器上，因此它完全不可信。**

## 2. 四层防御体系

### 第一层：传输通道加密 (防窃听)
*   **推荐方案**: **AES-GCM-256** 或 **ChaCha20-Poly1305**。
*   **握手流程**:
    1.  Client -> Server: `Hello { ClientRandom, PubKey_C }` (ECDH)
    2.  Server -> Client: `Welcome { ServerRandom, PubKey_S }` (ECDH)
    3.  双方计算共享密钥 `SharedSecret`。
    4.  派生会话密钥 `SessionKey`。

### 第二层：数据包签名与校验 (防篡改)
*   **HMAC**: 关键指令（如 `Fire`）附带 HMAC-SHA256 签名。
*   **Replay Protection**: 使用滑动窗口机制检查 `Sequence ID` 和 `Timestamp`，拒绝重放包。

### 第三层：ECS 逻辑校验 (EnTT Implementation)
利用 EnTT 系统在每一帧进行逻辑检查。

```cpp
// 移动校验系统
void MovementValidationSystem::update(entt::registry& registry, float dt) {
    auto view = registry.view<const Transform, const Velocity, PlayerState>();
    
    view.each([dt](const auto entity, const auto& transform, const auto& vel, auto& state) {
        // 1. 速度检查 (SpeedHack)
        float speed = glm::length(vel.linear);
        if (speed > MAX_SPEED * 1.1f) { // 允许 10% 误差
            // 触发回拉 (Rubberbanding)
            state.flags |= STATE_ILLEGAL_MOVE;
            LOG_WARN("SpeedHack detected: entity {}", entity);
        }
        
        // 2. 穿墙检查 (Noclip)
        // 使用上一次位置到当前位置发射射线
        if (physics_world.RayCast(state.last_pos, transform.pos)) {
             // 修正位置到墙前
             transform.pos = state.last_pos;
        }
    });
}
```

### 第四层：服务器端行为检测 (最终防线)
*   **射速检查**: 检查两次开火间隔是否小于武器最小间隔。
*   **自瞄检测**: 统计玩家准星的 Angular Velocity，如果瞬间锁定头部且无中间平滑过程，标记可疑。
*   **资源校验**: 局内金币由 C++ 权威计算，不接受客户端上报的扣款结果。

## 3. 总结

| 防御层级 | 针对威胁 | 技术手段 |
| :--- | :--- | :--- |
| **L1 传输层** | 网络抓包 | AES-GCM / ChaCha20 |
| **L2 协议层** | 篡改, 重放 | HMAC, SeqID, Timestamp |
| **L3 逻辑层** | 加速, 穿墙 | EnTT System 校验, Jolt RayCast |
| **L4 行为层** | 自瞄, 透视 | 统计学分析, 启发式检测 |
