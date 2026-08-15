<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Text Draw List

Text Draw List 是 Render Pipeline component 中保存逐帧已整形字形的容器。它不接收字符串，不负责
Unicode 解码、字体回退、换行、文字整形或字形栅格化。

## 公共入口

- C：`<granit/pipeline/text_draw_list.h>`。
- C++20：`<granit/pipeline/text_draw_list.hpp>`，使用 move-only 的 `granit::text_draw_list`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。
- R8 Atlas：`<granit/pipeline/text_atlas.h>` 或 C++20 的 `<granit/pipeline/text_atlas.hpp>`。

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

## R8 字形 Atlas

- Atlas 使用固定尺寸 R8 UNORM 页面，默认 `512×512`、最多 8 页、字形间保留 1 像素 Padding。
- 页面在首次需要时懒创建并清零，采用稳定的逐行 Shelf 分配；已经分配的字形不会因新增字形移动。
- `font_key + glyph_id` 是缓存键。重复上传只允许宽高和 bearing 完全一致，并原位更新覆盖率。
- 位图使用逐行 R8 数据；`bytes_per_row=0` 表示紧密排列。零宽且零高可登记空白字形，不分配页面。
- Atlas 达到 `max_pages` 后返回 `GRANIT_ERROR_OUT_OF_MEMORY`，当前不做隐式驱逐或重排。
- Atlas 拥有内部 Texture 和 View；上传与销毁由调用方进行外部同步。

## 当前限制

- 当前已支持 R8 Atlas 上传，但尚未公开 Text Draw List 到 Canvas 的转换接口。
- `font_key` 不可持久化为 Granit 资源身份，其含义和稳定期由调用方定义。
- 首版不会直接引入 FreeType、HarfBuzz 或平台字体 API。
- 通用 Canvas Shader 不能正确解释 R8 覆盖率；下一步将增加专用文字遮罩采样路径，而不是把 Atlas
  膨胀为 RGBA8。
