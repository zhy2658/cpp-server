# 运维与监控标准 (Ops & Monitoring)

## 1. 部署架构 (Deployment)

*   **容器化**: 全面 Docker 化。
*   **编排**: Kubernetes (K8s) 或 Docker Compose (单机)。
*   **镜像**:
    *   `cpp-server:latest` (基于 Alpine/Debian Slim, 仅包含二进制与依赖库)
    *   `go-server:latest`

## 2. 监控指标 (Prometheus Metrics)

C++ 服务器需暴露 HTTP 接口 (如 `:9090/metrics`) 供 Prometheus 抓取。

### 2.1 核心指标
*   **`server_tick_duration_ms`**: (Histogram) 主循环耗时。**P99 必须 < 16ms**。
*   **`online_players`**: (Gauge) 当前在线玩家数。
*   **`active_rooms`**: (Gauge) 当前活跃房间数。
*   **`network_in_bytes` / `network_out_bytes`**: (Counter) 流量吞吐。
*   **`kcp_resend_count`**: (Counter) KCP 重传包数 (监控网络质量)。

## 3. 日志规范 (Logging Standard)

使用 `spdlog` 异步日志，统一 JSON 格式以便 ELK 采集。

*   **Level 定义**:
    *   `DEBUG`: 详细的调试信息 (仅开发环境开启)。
    *   `INFO`: 关键流程 (玩家进出、房间创建、结算)。
    *   `WARN`: 逻辑异常但不影响运行 (如收到非法包、丢弃过期包)。
    *   **`ERROR`**: 需要人工介入的错误 (配置加载失败、Redis 断连)。**必须触发报警**。
    *   `CRITICAL`: 进程无法继续运行 (OOM, 核心服务不可用)。

### 3.1 日志示例
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

## 4. 报警策略 (Alerting)
*   **CPU 使用率 > 80%** (持续 1min)。
*   **内存使用率 > 90%** (OOM 预警)。
*   **Tick P99 > 30ms** (严重的服务器卡顿)。
*   **Error 日志速率 > 10/s** (异常爆发)。
