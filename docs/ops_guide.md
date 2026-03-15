# 运维与监控标准 (Ops & Monitoring)

## 1. 部署架构 (Deployment)

*   **容器化**: 全面 Docker 化。
*   **编排**: Kubernetes (K8s) 或 Docker Compose (单机)。
*   **镜像**:
    *   `cpp-server:latest` (基于 Alpine/Debian Slim, 仅包含二进制与依赖库)
    *   `go-server:latest`

## 2. 配置文件结构 (Configuration)

C++ 服务器启动时默认加载 `config.yaml`。

```yaml
server:
  ip: "0.0.0.0"
  port: 8888
  thread_pool_size: 4
  tick_rate: 60
  prometheus_port: 9090

kcp:
  nodelay: 1    # 1: 启用 nodelay
  interval: 10  # 内部时钟 (ms)
  resend: 2     # 快速重传
  nc: 1         # 关闭流控
  sndwnd: 512   # 发送窗口
  rcvwnd: 512   # 接收窗口
  mtu: 1200     # 考虑 Internet 1500 - UDP头 - IP头

log:
  level: "info" # debug, info, warn, error
  path: "logs/server.log"
  max_size: 10485760 # 10MB
  max_files: 5
```

## 3. 平滑关闭 (Graceful Shutdown)

为了避免战斗中途服务器重启导致玩家掉线，必须实现平滑关闭流程。

1.  **信号捕获**: 监听 `SIGTERM` / `SIGINT`。
2.  **拒绝新连接**: 将 `ServerStatus` 设为 `Draining`，拒绝新的 `JoinRoom` 请求。
3.  **等待结束**: 维持现有房间的 Tick 循环，直到所有房间自然结束 (GameOver)。
4.  **强制超时**: 若超过 30分钟仍未结束，强制踢出所有玩家并保存数据。
5.  **退出进程**: 释放资源，退出 `main` 函数。

## 4. 监控指标 (Prometheus Metrics)

C++ 服务器需暴露 HTTP 接口 (如 `:9090/metrics`) 供 Prometheus 抓取。

### 4.1 核心指标
*   **`server_tick_duration_ms`**: (Histogram) 主循环耗时。**P99 必须 < 16ms**。
*   **`online_players`**: (Gauge) 当前在线玩家数。
*   **`active_rooms`**: (Gauge) 当前活跃房间数。
*   **`network_in_bytes` / `network_out_bytes`**: (Counter) 流量吞吐。
*   **`kcp_resend_count`**: (Counter) KCP 重传包数 (监控网络质量)。

## 5. 日志规范 (Logging Standard)

使用 `spdlog` 异步日志，统一 JSON 格式以便 ELK 采集。

*   **Level 定义**:
    *   `DEBUG`: 详细的调试信息 (仅开发环境开启)。
    *   `INFO`: 关键流程 (玩家进出、房间创建、结算)。
    *   `WARN`: 逻辑异常但不影响运行 (如收到非法包、丢弃过期包)。
    *   **`ERROR`**: 需要人工介入的错误 (配置加载失败、Redis 断连)。**必须触发报警**。
    *   `CRITICAL`: 进程无法继续运行 (OOM, 核心服务不可用)。

### 5.1 日志示例
```json
{
  "time": "2023-10-27T10:00:00Z",
  "level": "INFO",
  "module": "Room",
  "room_id": 1001,
  "msg": "Player 12345 joined room",
  "player_count": 5
}
```

## 6. 报警策略 (Alerting)
*   **CPU 使用率 > 80%** (持续 1min)。
*   **内存使用率 > 90%** (OOM 预警)。
*   **Tick P99 > 30ms** (严重的服务器卡顿)。
*   **Error 日志速率 > 10/s** (异常爆发)。
