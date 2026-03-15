# ECS 组件定义规范 (ECS Components)

本文档定义 `cpp-server` 战斗服中 EnTT 所使用的核心 Component，是 Layer 4 (Gameplay) 开发的标准参考。

## 1. 设计原则

*   **纯数据结构**：Component 不包含方法，仅存储数据。
*   **无指针**：跨实体引用只存 `entt::entity` (uint32_t)。
*   **命名**：使用 `PascalCase`，与 EnTT 惯例一致。

## 2. 核心组件清单

### 2.1 基础空间组件

| 组件名 | 字段 | 用途 |
| :--- | :--- | :--- |
| **Transform** | `glm::vec3 pos`, `float yaw`, `float pitch` | 世界坐标与朝向，所有可移动实体的基础 |
| **Velocity** | `glm::vec3 linear`, `glm::vec3 angular` | 线速度、角速度，物理系统写入 |
| **PhysicsBody** | `JPH::BodyID body_id` | Jolt 刚体 ID，用于 Raycast/Collision 实体映射 |

### 2.2 玩家相关

| 组件名 | 字段 | 用途 |
| :--- | :--- | :--- |
| **PlayerState** | `uint64_t user_id`, `uint32_t conv`, `uint32_t flags`, `glm::vec3 last_pos` | 玩家身份、KCP Conv、状态标志、上一帧位置（校验用） |
| **Health** | `int32_t current`, `int32_t max`, `int32_t armor` | 血量、护甲，CombatSystem 读写 |
| **InputState** | `glm::vec3 move_dir`, `float yaw`, `float pitch`, `uint32_t buttons`, `uint32_t last_seq` | 最近收到的输入，用于移动与校验 |

### 2.3 战斗与武器

| 组件名 | 字段 | 用途 |
| :--- | :--- | :--- |
| **Weapon** | `uint32_t weapon_id`, `uint8_t weapon_type` (0=Ranged, 1=Melee), `int32_t ammo`, `float next_fire_time`, `float attack_range`, `float hit_radius` | 武器 ID、类型、弹药、射速冷却；远程用 ammo/next_fire_time，近战用 attack_range/hit_radius |
| **Hitbox** | `uint8_t body_part` (Head=0, Chest=1, Stomach=2, Legs=3) | 命中部位系数映射 |

**武器类型说明**：
*   **Ranged (0)**：`ammo` 有效，`next_fire_time` 控制射速，判定采用 Raycast。
*   **Melee (1)**：`ammo` 可为 -1（无限），`attack_range` 与 `hit_radius` 用于球体/盒体 Overlap 检测。

### 2.4 延迟补偿

| 组件名 | 字段 | 用途 |
| :--- | :--- | :--- |
| **TransformSnapshot** | `std::vector<std::pair<uint32_t, Transform>>` | 历史 Transform 快照 (时间戳 -> 状态) |

### 2.5 标签组件 (Tag)

| 组件名 | 说明 |
| :--- | :--- |
| **PlayerTag** | 标识玩家实体 |
| **ProjectileTag** | 标识子弹/投掷物 |
| **StaticObstacleTag** | 静态碰撞体 |

## 3. 系统与组件依赖

| 系统 | 读取组件 | 写入组件 |
| :--- | :--- | :--- |
| MovementSystem | Transform, Velocity, InputState | Transform |
| CombatSystem | Transform, Health, Weapon, PhysicsBody | Health, Weapon |
| MovementValidationSystem | Transform, Velocity, PlayerState | PlayerState, Transform |
| LagCompensationSystem | TransformSnapshot | （临时回滚，不持久写入） |
