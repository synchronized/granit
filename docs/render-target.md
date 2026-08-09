<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Render Target Attachment

## 定位

`granit/render_target.h` 定义颜色和深度/模板 Attachment 的后端无关值类型。普通离屏 Texture
View 与 Swapchain Backbuffer View 使用同一套描述，公共接口不暴露 Vulkan Layout 或附件类型。

Command Recorder 已提供 Dynamic Rendering 的 begin/end，并直接复用这些描述。资源 Layout 和
颜色及深度/模板 Attachment 的跨命令屏障由 F-05 自动接入，并按实际 Queue 提交顺序解析 Layout。

## C API 值类型

颜色附件默认清除为不透明黑色并在渲染结束后保留：

```c
granit_color_attachment_desc color = GRANIT_COLOR_ATTACHMENT_DESC_INIT;
color.view = color_view;
color.clear_value.red = 0.1f;
color.clear_value.green = 0.2f;
color.clear_value.blue = 0.3f;
```

深度/模板附件默认清除深度为 `1.0` 并保存深度；模板默认 discard：

```c
granit_depth_stencil_attachment_desc depth =
    GRANIT_DEPTH_STENCIL_ATTACHMENT_DESC_INIT;
depth.view = depth_view;
```

两个描述的第一版 ABI 大小均为 48 字节。调用者必须使用初始化宏或正确填写 `struct_size`，
保留字段必须为零。

## Load 与 Store

Load 支持：

- `LOAD`：读取附件原有内容。
- `CLEAR`：使用描述中的清除值。
- `DISCARD`：不保留此前内容。

Store 支持：

- `STORE`：保留本次渲染结果。
- `DISCARD`：结束后不保证内容。

`UNDEFINED` 只用于发现未初始化描述，不能作为有效操作。Swapchain Backbuffer 在 present 前需要
保留内容，后续 F-06 会拒绝不适合 present 的 store 行为。

## C++20 包装

`granit/render_target.hpp` 提供：

- `attachment_load_operation` 和 `attachment_store_operation` 强类型枚举。
- `clear_color_value` 和 `clear_depth_stencil_value`。
- `color_attachment_desc` 和 `depth_stencil_attachment_desc`。
- `native()` 转换，用于轻量映射到 C ABI，不维护额外运行时状态。

## 当前验证范围

纯值验证会拒绝：

- 空 View。
- 未知或 `UNDEFINED` load/store 操作。
- 非零保留字段。
- NaN 或无穷清除值。
- 小于 `0` 或大于 `1` 的深度清除值。

View 的 Renderer domain、格式、Texture usage、尺寸、样本数和附件间一致性需要访问 Registry，
将在 F-02 的命令入口校验。

详细设计见 [R-09 计划](plans/R-09-render-target-attachment.md)。
