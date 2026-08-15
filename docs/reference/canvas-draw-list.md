<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Canvas Draw List

Canvas Draw List 是 Render Pipeline component 中面向 UI 后端、Sprite、文字和调试覆盖层的逐帧几何容器。
它只保存已经布局完成的三角形，不提供控件树、布局、输入或字体处理。

## 公共入口

- C：`<granit/pipeline/canvas_draw_list.h>`。
- C++20：`<granit/pipeline/canvas_draw_list.hpp>`，使用 move-only 的 `granit::canvas_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

当前公开范围是 Draw List 构建、复用和统计。把列表录制到 UI Pass 的公共接口仍在 H-08B 设计中；
内部 UI Pass 不是兼容承诺，也不应由使用者直接包含。

## 坐标与顶点

- `granit_canvas_vertex` 包含二维像素位置、UV 和打包 RGBA8 UNORM 顶点色。
- 屏幕坐标原点位于输出左上角，X 向右、Y 向下。
- 通用追加接口接收三角形列表，索引必须相对本次传入的顶点数组。
- 矩形便捷接口生成四个顶点和六个索引，宽高必须大于零。
- 位置和 UV 必须是有限浮点数；列表在函数返回前复制全部数组。

## 状态与合批

每项借用一个 Texture View、Sampler、Scissor 和逻辑 Layer。Texture View 与 Sampler 必须保持有效，
直到列表完成录制；列表销毁时不会销毁它们。

实现只合并相邻且四项状态完全相同的 Item，不跨项重排透明内容。`get_stats` 返回顶点、索引、Item
以及合批后的 Batch 数，可用于适配层诊断和性能回归。

## 生命周期与线程安全

- Draw List 创建时关联一个 Renderer，跨 Renderer 调用返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `reset` 清空当帧内容但保留动态容量，推荐每帧复用同一个列表。
- 不同 Draw List 可以由不同线程并行构建。
- 同一个 Draw List 的 append、reset、统计和后续录制操作需要调用方进行外部同步。
- 销毁后旧句柄通过 generation 失效；重复销毁返回无效句柄。

## 最小 C 示例

```c
granit_canvas_draw_list_desc list_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
granit_canvas_draw_list list = GRANIT_NULL_HANDLE;
granit_result result = granit_canvas_draw_list_create(renderer, &list_desc, &list);

granit_canvas_rect_desc rect = GRANIT_CANVAS_RECT_DESC_INIT;
rect.x = 10.0f;
rect.y = 20.0f;
rect.width = 100.0f;
rect.height = 40.0f;
rect.state.texture = texture_view;
rect.state.sampler = sampler;
if (result == GRANIT_SUCCESS)
  result = granit_canvas_draw_list_append_rect(renderer, list, &rect);

/* H-08B 将提供公共录制/提交接口。 */
granit_canvas_draw_list_destroy(renderer, list);
```
