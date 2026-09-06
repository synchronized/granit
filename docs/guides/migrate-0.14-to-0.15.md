<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.14 迁移到 0.15

0.15.0 新增异步 Readback Batch、Pipeline Warmup Batch 和对应能力位。既有同步 Texture Readback
仍保留，但截图、拾取及离屏导出应迁移到异步接口。

## 必要调整

- 将 `find_package(granit 0.14)` 更新为 `find_package(granit 0.15)` 并重新编译 Consumer。
- 若初始化 `granit_renderer_resource_stats`，继续使用 `GRANIT_RENDERER_RESOURCE_STATS_INIT`，不要
  手写结构大小；新版本尾部增加 `pipeline_warmup_batch_count`。
- 将长耗时的 `texture.read()` 改为 Readback Batch 提交、轮询、结果信息查询和显式复制。
- Pipeline 预热前查询能力；只有
  `GRANIT_RENDERER_FEATURE_NON_BLOCKING_PIPELINE_WARMUP_BIT` 存在时，才可假定单次推进不触发同步
  编译。

## 所有权变化

Readback Batch 提交后可以立即重置并复用；操作结果由异步操作对象持有。Pipeline Warmup Batch
复制描述数组，但引用的 Shader 与 Pipeline Layout 必须活到操作结束。两类公开批次都由调用方销毁，
并计入 Renderer 资源统计。
