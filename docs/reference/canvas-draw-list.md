<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Canvas Draw List

Canvas Draw List 是 Render Pipeline component 中面向 UI 后端、Sprite、文字和调试覆盖层的逐帧几何容器。
它只保存已经布局完成的三角形，不提供控件树、布局、输入或字体处理。

## 公共入口

- C：`<granit/pipeline/canvas_draw_list.h>`。
- C++20：`<granit/pipeline/canvas_draw_list.hpp>`，使用 move-only 的 `granit::canvas_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

当前公开范围包括 Draw List 构建、复用、统计以及录制到已有 Command Recorder。内部 Canvas
Material、动态几何上传和 Canvas Pass 均不是兼容承诺，也不应由使用者直接包含。

## 坐标与顶点

- `granit_canvas_vertex` 包含二维位置、UV 和打包 RGBA8 UNORM 顶点色。
- Draw List 本身不解释位置的坐标空间；公共录制接口使用左上原点、Y 向下的像素坐标。
- 通用追加接口接收三角形列表，索引必须相对本次传入的顶点数组。
- 批量追加接口用一组 `granit_canvas_draw_range` 引用共享顶点和索引数组，适合 UI 后端一次提交
  整帧几何；范围按索引顺序排列、互不重叠且不得越界。
- 矩形便捷接口生成四个顶点和六个索引，宽高必须大于零。
- 位置和 UV 必须是有限浮点数；列表在函数返回前复制全部数组。

## 状态与合批

每项借用一个 Texture View、Sampler 和 Scissor。Texture View 与 Sampler 必须保持有效，直到列表完成
录制；列表销毁时不会销毁它们。

实现只合并相邻且三项状态完全相同的 Item，不跨项重排透明内容。`get_stats` 返回顶点、索引、Item
以及合批后的 Batch 数，可用于适配层诊断和性能回归。

## 生命周期与线程安全

- Draw List 创建时关联一个 Renderer，跨 Renderer 调用返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `clear` 清空当帧内容但保留动态容量，推荐每帧复用同一个列表。
- 不同 Draw List 可以由不同线程并行构建。
- 同一个 Draw List 的 append、clear、统计和后续录制操作需要调用方进行外部同步。
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

granit_canvas_record_desc record = GRANIT_CANVAS_RECORD_DESC_INIT;
record.color = output_view;
record.color_format = GRANIT_TEXTURE_FORMAT_RGBA8_SRGB;
record.width = 1280;
record.height = 720;
record.encode_srgb = 0; /* sRGB Attachment 由硬件编码；UNORM 显示目标通常设为 1。 */
record.frame_slot = frame_index % GRANIT_CANVAS_FRAME_SLOT_COUNT;
result = granit_canvas_draw_list_record(renderer, recorder, list, &record);
granit_canvas_draw_list_destroy(renderer, list);
```

## 录制语义

- Recorder 必须已经 begin；函数只录制命令，不结束或提交 Recorder。
- `LOAD` 保留目标原有内容，适合覆盖层；`CLEAR` 和 `DISCARD` 用于独立 Canvas Pass。
- `encode_srgb=1` 在 Shader 中编码 RGB，适合已经保存 sRGB 显示值的 UNORM 目标；sRGB Attachment
  应保持为零并交由硬件编码。
- 当前列表持有并复用内部 Material 与上传 Buffer；同一列表的录制仍需调用方外部同步。
- 动态几何使用三个持久映射的 Upload Buffer 槽。`frame_slot` 应与调用方的 Recorder 帧槽一致，
  并仅在该 Recorder 已执行完成后复用；`GRANIT_CANVAS_FRAME_SLOT_AUTO` 保留兼容的自动轮转行为。
  容量只在当前槽不足时按二次幂增长。
