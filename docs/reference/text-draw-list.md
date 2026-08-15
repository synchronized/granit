<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Text Draw List

Text Draw List 是 Render Pipeline component 中保存逐帧已整形字形的容器。它不接收字符串，不负责
Unicode 解码、字体回退、换行、文字整形或字形栅格化。

## 公共入口

- C：`<granit/pipeline/text_draw_list.h>`。
- C++20：`<granit/pipeline/text_draw_list.hpp>`，使用 move-only 的 `granit::text_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

## 字形语义

- `font_key` 是调用方定义的非零 64 位身份，应覆盖字体、字号和会改变位图的栅格化参数。
- `glyph_id` 是整形器输出的字体内字形编号；零值可用于字体的 `.notdef` 字形。
- `x`、`y` 是有限浮点基线坐标，采用 Canvas 的左上原点、Y 轴向下像素空间。
- `color` 是打包 RGBA8 UNORM；同一 Run 可以为每个字形提供不同颜色。
- Text Draw List 复制字形数组，不借用调用方内存；Run 保存独立 Scissor，全零表示不裁剪。

## 生命周期与线程安全

- 列表与创建它的 Renderer 关联，跨 Renderer 使用返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `clear` 清空字形与 Run，同时保留容量以供后续帧复用。
- 不同列表可以由不同线程构建；同一列表由调用方进行外部同步。
- 销毁后旧句柄通过 generation 失效。

## 当前限制

- 当前只保存已整形字形及 Run，尚未公开 Atlas 上传或 Canvas 转换接口。
- `font_key` 不可持久化为 Granit 资源身份，其含义和稳定期由调用方定义。
- 首版不会直接引入 FreeType、HarfBuzz 或平台字体 API。
