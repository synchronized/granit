<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Graphics Pipeline

Granit 当前提供 Graphics Pipeline、Bind Group Layout 和不可变 Bind Group。公共接口不暴露
Vulkan Pipeline、Pipeline Layout 或 Dynamic Rendering 结构。

## 当前能力

- 创建 Bind Group Layout，并由 Pipeline Layout 按组序号组合零至八个布局。
- 使用 Buffer、Texture View 和 Sampler 创建不可变 Bind Group。
- 使用 Vertex Shader、Fragment Shader、颜色格式、可选深度模板格式和样本数创建 Pipeline。
- 支持点、线和三角形拓扑，以及正面绕序、剔除模式和 Fill/Line/Point 多边形模式。
- 支持深度测试、深度写入和比较操作，以及每个颜色附件独立的混合与写入掩码。
- Viewport 与 Scissor 是动态状态，将由 D-05 的命令接口设置。
- Pipeline 内部保持 Shader 与 Layout 存活；对应公开句柄可以先销毁。
- 所有对象支持 Renderer domain、generation、Device Lost、级联诊断和延迟销毁。

Bind Group 创建时必须完整提供 Layout 声明的每个 binding 数组元素，创建后不能原地修改。Granit
内部为其管理 Descriptor Pool 和 Descriptor Set，并保持所有绑定资源存活。
当前每个 Bind Group 使用独立的内部 Descriptor Pool，以优先保证回收和并发语义清晰；后续可以在
不改变公共 API 的前提下改为分块池。

Command Recorder 可以绑定 Graphics Pipeline，并按连续组范围批量绑定 Bind Group。绑定时要求
Bind Group 的 Layout 与 Pipeline Layout 对应组使用同一个布局对象；仅字段相同不视为兼容。

Viewport、Scissor、Vertex/Index Buffer、Draw 和 Draw Indexed 已经实现。Graphics Pipeline
支持为每个 Vertex Buffer binding 指定 stride、per-vertex/per-instance 步进，以及 location、
format 和 offset。未提供 Vertex Buffer Layout 时仍可使用 Shader 内的顶点序号生成位置。
非实心 Line/Point 模式依赖设备能力；设备不支持时，Pipeline 创建返回“不支持”。
未显式提供深度状态时，有深度格式的 Pipeline 默认启用测试和写入并使用 Less Or Equal；未提供
颜色混合状态时默认关闭混合并写入 RGBA。

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
内部生命周期；用户可以在 Pipeline 创建后释放对应公共句柄。Command Recorder 支持绑定 Compute
Pipeline、Compute Bind Group 并直接 Dispatch；Dispatch 只能在 Dynamic Rendering 区域外执行。

Compute Bind Group 的资源状态由 Granit 自动准备。Uniform Buffer 与 Sampled Texture 为只读，
Storage Buffer 与 Storage Texture 保守按读写处理；每次 Dispatch 前生成所需访问状态，后续 Copy
或渲染命令会自动衔接跨阶段屏障。

创建函数会复制格式数组。Pipeline 和 Layout 均由创建它们的 Renderer 管理，不能跨 Renderer
混用或销毁。
