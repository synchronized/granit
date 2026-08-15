<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Debug Draw List

Debug Draw List 是 Render Pipeline component 中可复用的逐帧调试命令容器。它保存线段和三角形，
不进入底层 Renderer，也不负责持久化、编辑器 Gizmo 状态或碰撞调试数据来源。

## 公共入口

- C：`<granit/pipeline/debug_draw_list.h>`。
- C++20：`<granit/pipeline/debug_draw_list.hpp>`，使用 move-only 的 `granit::debug_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

H-08C1 提供命令构建、复用和统计；H-08C2 已支持把屏幕空间命令追加到 Canvas Draw List。世界空间
Unlit 录制与参考管线提交将在后续步骤中完成。

## 命令语义

- 顶点位置使用三个有限浮点数，颜色使用打包 RGBA8 UNORM。
- 线宽使用像素单位且必须大于零；实际粗线展开策略尚未进入公共承诺。
- `WORLD` 命令允许关闭或启用深度测试；`SCREEN` 命令固定关闭深度测试。
- append 接口批量复制调用方数组，返回后不再借用输入内存。
- 列表保持追加顺序，不自动跨命令重排。

## 屏幕空间转换

- `append_screen_to_canvas` 把屏幕空间线段展开为四顶点、六索引的矩形，把三角形直接复制到 Canvas。
- 一次转换只追加一个 Canvas Item，并保留原始屏幕命令顺序；世界空间命令不会进入该 Item。
- Debug Draw List 内部懒创建白纹理和 Sampler，Canvas 完成录制前必须保持该列表有效。
- 转换只追加而不清空目标 Canvas，允许调用方组合 UI、文字和多份调试列表。

## 生命周期与线程安全

- 列表与创建它的 Renderer 关联，跨 Renderer 使用返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `clear` 清空当前命令并保留容量，适合每帧复用。
- 不同列表可以由不同线程构建；同一列表需要调用方外部同步。
- 销毁后旧句柄通过 generation 失效。

## 当前限制

- 尚未提供 Box、Sphere、Axes、Frustum 等便捷 Gizmo 生成函数。
- 世界空间线段算法尚未进入公共录制接口或兼容承诺。
- 屏幕空间命令可沿 Canvas 路径录制；世界空间命令仍不能直接产生 GPU Draw。

## 世界空间线段算法状态

H-08C3a 已在内部固定以下规则，但尚未公开录制接口：

- 使用 Vulkan 齐次裁剪范围 `-w≤x≤w`、`-w≤y≤w`、`0≤z≤w`，并拒绝非正 `w`。
- 线段先在齐次空间裁剪，再透视除法；跨近面的端点颜色按裁剪参数插值。
- 在线段屏幕方向的法线上按视口尺寸展开，因此透视变化时仍保持请求的像素宽度。
- 完全位于裁剪体外或投影后退化为单点的线段不生成几何。
