<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-115 RenderPipeline 可选通道 GPU 计时验收

## 范围

验证没有方向光、因而没有阴影 Pass 的 RenderPipeline，不会因为固定 8 查询回读中的阴影查询未写入而
永久保持 pending。测试同时覆盖有方向光与无方向光场景的交替离屏渲染。

## 根因确认

RenderPipeline 指标池固定分配 8 个查询；无阴影时查询 0/1 原先没有录制，但回读仍请求 0～7。Vulkan
非等待回读因此持续返回 `GRANIT_ERROR_NOT_READY`，指标槽位不能重新投入使用。

## 修复与证据

- 缺省阴影通道在指标录制开始阶段写入查询 0/1；
- 集成测试交替提交有光照和无光照场景，并检查样本序号增长；
- 无光照样本检查 `shadow_gpu_ns == 0`；
- Windows Clang 配置和增量构建通过；
- 聚焦测试 `granit.pipeline.render`、`granit.pipeline.api` 和 `granit.documentation.links` 通过；
- 全量 CTest 通过，119/119。

## 发布状态

**本地验证完成，未发布。** 本记录对应 v0.72.0 发布候选，仍需完成远端候选验收后才可创建 tag 或
GitHub Release。
