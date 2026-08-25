<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 第三方 UI 与字体适配

Granit 不接管第三方 UI 的控件树、输入或字体系统。适配器只在每帧把第三方输出转换成 Canvas 或
Text 输入，第三方库不成为 Renderer 的传递依赖。

## 立即式 UI Draw Data

每帧第三方 Draw Data 按原始顺序批量转换：

1. 将所有位置、UV 和 RGBA8 颜色逐字段复制到一个 `granit_canvas_vertex` 数组。
2. 将所有索引复制到一个数组，每条命令只生成一个 `granit_canvas_draw_range`。
3. 通过应用自己的映射表，把第三方 Texture ID 解析为 Granit Texture View 和 Sampler。
4. 将 Clip Rect 转换为左上原点、Y 轴向下的 `granit_scissor`。
5. 调用一次 `granit_canvas_draw_list_append_batch`；Canvas 只合并相邻且 Texture、Sampler、
   Scissor 相同的范围。

不要直接 `reinterpret_cast` 第三方顶点数组，也不要把指针强制转换成 Granit 句柄。Texture ID 的
含义、注册和失效由适配器管理；被借用的 View 与 Sampler 至少存活到 Canvas 完成录制。

仓库中的 `granit_immediate_ui_adapter_example` 使用一个仿立即式 UI Draw Data 结构验证上述转换，
不要求安装 ImGui、Nuklear 或其他 UI 库。接入具体库时只需在应用层实现相同映射。

不要为每条命令重新截取和编号顶点。顶点在整帧转换中只复制一次，Draw Command 只保存纹理、
裁剪区和索引范围，避免细粒度 C ABI 调用与重复句柄校验。

## 字体整形与栅格化

- HarfBuzz、平台排版器或 UI 框架负责输出 `glyph_id` 和基线位置。
- FreeType、平台字体 API 或 UI 框架负责生成 R8 Coverage 位图及 bearing。
- 适配器为字体、字号和栅格化参数生成稳定的非零 `font_key`。
- 位图通过 Text Atlas 上传；已定位字形批量追加到 Text Draw List，再一次转换到 Canvas。
- 字体对象和第三方 Glyph Cache 仍由上层持有，Granit 只拥有自己的 Atlas 页面。

首版不提供强制的 ImGui、HarfBuzz 或 FreeType 构建选项。具体 Adapter 可位于应用仓库或未来独立
CMake component，不能进入核心 C ABI，也不能把第三方类型写入 Granit 公共头文件。

## 线程与每帧顺序

- 第三方 UI 可以在自己的线程生成 Draw Data，但同一 Granit Draw List 需要调用方外部同步。
- 在资源销毁前完成 Texture ID 注销，并确保已提交 GPU 工作不再引用对应资源。
- 推荐每帧先 `clear`，再完成全部转换，最后把一个 Canvas 绑定给 Render Pipeline 输出。
