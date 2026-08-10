<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Graphics Pipeline

Granit 当前提供 Graphics Pipeline、Bind Group Layout 和不可变 Bind Group。公共接口不暴露
Vulkan Pipeline、Pipeline Layout 或 Dynamic Rendering 结构。

## 当前能力

- 创建 Bind Group Layout，并由 Pipeline Layout 按组序号组合零至八个布局。
- 使用 Buffer、Texture View 和 Sampler 创建不可变 Bind Group。
- 使用 Vertex Shader、Fragment Shader、颜色格式、可选深度模板格式和样本数创建 Pipeline。
- 固定采用三角形列表、填充模式、关闭剔除和逆时针正面。
- Viewport 与 Scissor 是动态状态，将由 D-05 的命令接口设置。
- Pipeline 内部保持 Shader 与 Layout 存活；对应公开句柄可以先销毁。
- 所有对象支持 Renderer domain、generation、Device Lost、级联诊断和延迟销毁。

Bind Group 创建时必须完整提供 Layout 声明的每个 binding 数组元素，创建后不能原地修改。Granit
内部为其管理 Descriptor Pool 和 Descriptor Set，并保持所有绑定资源存活。
当前每个 Bind Group 使用独立的内部 Descriptor Pool，以优先保证回收和并发语义清晰；后续可以在
不改变公共 API 的前提下改为分块池。

Command Recorder 可以绑定 Graphics Pipeline，并按连续组范围批量绑定 Bind Group。绑定时要求
Bind Group 的 Layout 与 Pipeline Layout 对应组使用同一个布局对象；仅字段相同不视为兼容。

Viewport、Scissor、Vertex/Index Buffer、Draw 和 Draw Indexed 已经实现。第一版尚未开放
Vertex Attribute Layout，最小三角形使用 Shader 内的顶点序号生成位置。

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

Compute Pipeline 使用独立句柄，但复用 Pipeline Layout：

```c
granit_compute_pipeline_desc compute_desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
compute_desc.layout = pipeline_layout;
compute_desc.compute_shader = compute_shader;

granit_compute_pipeline compute_pipeline = GRANIT_NULL_HANDLE;
granit_compute_pipeline_create(renderer, &compute_desc, &compute_pipeline);
```

Compute Shader 必须以 `GRANIT_SHADER_STAGE_COMPUTE` 创建。Pipeline 会保持 Layout 和 Shader 的
内部生命周期；用户可以在 Pipeline 创建后释放对应公共句柄。Dispatch 命令将在 D-07B 接入。

创建函数会复制格式数组。Pipeline 和 Layout 均由创建它们的 Renderer 管理，不能跨 Renderer
混用或销毁。
