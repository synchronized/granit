<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Debug Draw List

Debug Draw List 是 Render Pipeline component 中可复用的逐帧调试命令容器。它保存线段和三角形，
不进入底层 Renderer，也不负责持久化、编辑器 Gizmo 状态或碰撞调试数据来源。

## 公共入口

- C：`<granit/pipeline/debug_draw_list.h>`。
- C++20：`<granit/pipeline/debug_draw_list.hpp>`，使用 move-only 的 `granit::debug_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

H-08C1 当前只提供命令构建、复用和统计。世界空间 Unlit 录制、屏幕空间 Canvas 转换以及参考管线
提交将在 H-08C 后续步骤中完成。

## 命令语义

- 顶点位置使用三个有限浮点数，颜色使用打包 RGBA8 UNORM。
- 线宽使用像素单位且必须大于零；实际粗线展开策略尚未进入公共承诺。
- `WORLD` 命令允许关闭或启用深度测试；`SCREEN` 命令固定关闭深度测试。
- append 接口批量复制调用方数组，返回后不再借用输入内存。
- 列表保持追加顺序，不自动跨命令重排。

## 生命周期与线程安全

- 列表与创建它的 Renderer 关联，跨 Renderer 使用返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `clear` 清空当前命令并保留容量，适合每帧复用。
- 不同列表可以由不同线程构建；同一列表需要调用方外部同步。
- 销毁后旧句柄通过 generation 失效。

## 当前限制

- 尚未提供 Box、Sphere、Axes、Frustum 等便捷 Gizmo 生成函数。
- 尚未定义世界空间线段在近裁剪面后的展开方式。
- 当前没有录制或参考管线提交接口，命令列表还不能直接产生 GPU Draw。
