<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92A Frame Trace 基础设施

## 范围

本阶段为 v0.56.0 建立内部帧诊断基础设施，不新增公共 API，也不引入 RenderDoc、Vulkan 或 Dawn
作为 Granit 的公共依赖。Frame Trace 默认关闭，仅在开发诊断模式下收集现有诊断事件。

## 实现

- `src/core/frame_trace.{h,cpp}` 提供线程安全的内部事件写入器；
- 通过 `GRANIT_FRAME_TRACE` 指定 JSONL 输出路径；
- 通过 `GRANIT_FRAME_TRACE_MAX_EVENTS` 设置固定容量，默认 4096 条；
- 超出容量时保留最新事件，并在输出末尾记录 `dropped_events`；
- 诊断 sink 自动写入 `diagnostic` 事件，消息中的 JSON 特殊字符会转义；
- 进程退出时自动 flush，文件系统失败不会影响渲染和原有诊断回调；
- 测试专用配置入口只存在于内部实现，未进入安装头文件或 C ABI。

当前事件字段包含 `sequence`、`timestamp_ns`、`kind` 以及事件 payload。Frame、Pass、Resource、Sync
和 GPU Timestamp 的统一关联留给 S-92C，避免在首版基础设施中重复建立运行时状态。

## 验证

- `clang-format --dry-run --Werror`：通过；
- `git diff --check`：通过；
- `cmake --build build/windows-clang-debug --target granit_lifecycle_validation_test -j 4`：通过；
- `ctest --test-dir build/windows-clang-debug -R '^granit\\.core\\.lifecycle_validation$' --output-on-failure`：通过；
- 设置 `GRANIT_FRAME_TRACE` 后运行核心生命周期测试（排除 Frame Trace 自身的环境隔离用例）：通过，进程退出后生成 JSONL。

## 限制与后续

当前输出是开发期 JSONL，不承诺跨版本持久化兼容；写盘时机是显式 `flush` 或进程退出，尚未按帧
切分文件。下一阶段接入 Vulkan Debug Label/RenderDoc 动态探测，随后在 S-92C 关联资源统计、提交、
Fence 和 Timestamp 结果。
