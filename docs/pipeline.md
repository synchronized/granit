<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Graphics Pipeline

Granit 当前提供 Graphics Pipeline 和 Bind Group Layout，作为 D-03 的基础渲染路径。公共接口不暴露
Vulkan Pipeline、Pipeline Layout 或 Dynamic Rendering 结构。

## 当前能力

- 创建 Bind Group Layout，并由 Pipeline Layout 按组序号组合零至八个布局。
- 使用 Vertex Shader、Fragment Shader、颜色格式、可选深度模板格式和样本数创建 Pipeline。
- 固定采用三角形列表、填充模式、关闭剔除和逆时针正面。
- Viewport 与 Scissor 是动态状态，将由 D-05 的命令接口设置。
- Pipeline 内部保持 Shader 与 Layout 存活；对应公开句柄可以先销毁。
- 所有对象支持 Renderer domain、generation、Device Lost、级联诊断和延迟销毁。

第一版尚不能创建 Bind Group 或录制资源绑定，也不能录制 Pipeline 绑定或 Draw。不可变 Bind Group
与内部 Descriptor Pool 属于 D-03B 的下一步；绘制与固定功能状态将在 D-05 补充。

## C API 示例

```c
granit_pipeline_layout_desc layout_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
granit_pipeline_layout layout = GRANIT_NULL_HANDLE;
granit_pipeline_layout_create(renderer, &layout_desc, &layout);

const granit_texture_format formats[] = {GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
granit_graphics_pipeline_desc pipeline_desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
pipeline_desc.layout = layout;
pipeline_desc.vertex_shader = vertex_shader;
pipeline_desc.fragment_shader = fragment_shader;
pipeline_desc.color_format_count = 1;
pipeline_desc.color_formats = formats;

granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
granit_graphics_pipeline_create(renderer, &pipeline_desc, &pipeline);
```

创建函数会复制格式数组。Pipeline 和 Layout 均由创建它们的 Renderer 管理，不能跨 Renderer
混用或销毁。
