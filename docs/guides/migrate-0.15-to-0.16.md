<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.15 迁移到 0.16

0.16.0 不新增 C ABI 符号或结构字段，主要变化是异步提交的背压和 Pipeline 预热执行语义。

## 处理传输背压

Vulkan 的异步 Upload/Readback Batch 在后端槽位饱和时立即返回 `GRANIT_ERROR_NOT_READY`。批次
内容仍然有效；每帧调用 `granit_renderer_process_events` 后重试提交即可。不要销毁并重新构造批次，
也不要把 `NOT_READY` 记录为永久加载失败。

同步上传与普通 Pipeline 创建的行为没有变化。

## 判断 Pipeline 预热方式

继续先查询 `GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT`：

- 能力存在时，提交和事件推进不会执行同步冷编译，可以在交互加载循环中轮询。
- 能力不存在时，批量预热仍可使用，但应安排在允许短暂阻塞的加载阶段。

不要根据 Vulkan 或 WebGPU 后端名称推断该能力。
