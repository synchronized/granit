<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 06：添加材质与光照

本章把上一章的手写 Shader 绑定替换为 `.grmat` 材质归档，并加入方向光与基础 PBR。完成后，
旋转立方体的固有色、金属度和粗糙度由 Material 驱动，表面明暗由 Scene 中的相机与光源驱动。

Material 是参考 Render Pipeline 的输入，低层 Command Recorder 不公开它的内部 Bind Group。因此本章
同时首次接入 Render Pipeline；下一章会在这个最小用法上展开阴影、HDR、空场景和输出重建。

## 1. 在构建期生成资产

Shader 源仍然是 HLSL。CMake 先从 `pbr_standard.grshlib.json` 构建跨后端 Shader Library 和索引，
再用索引解析材质清单中的稳定 Shader Content ID，生成 `.grmat`：

```text
pbr_standard.hlsl + .grshlib.json → .grshlib + .grshidx.json
.grmat.json + .grshidx.json       → .grmat
```

运行时只读取 `.grshlib` 和 `.grmat`，不会调用 DXC、Tint 或解析工具侧 JSON。完整构建规则见
[`CMakeLists.txt`](../../examples/tutorials/06_material_and_lighting/CMakeLists.txt)。

## 2. 准备 PBR Mesh

PBR Material 声明了固定的顶点输入：Position、Normal、Tangent 和 UV。Mesh 的 stride、location 和
格式必须与材质归档一致。本章的立方体按面提供法线和切线，因此硬边不会被错误地平滑。

完整 CPU 模型数据位于
[`model_data.hpp`](../../examples/tutorials/06_material_and_lighting/model_data.hpp)。上传后的 Vertex 和
Index Buffer 仍由应用拥有，必须比借用它们的 Mesh 存活更久。

## 3. 创建 Material

示例用五张 1×1 默认纹理填满 PBR 材质声明的槽位，再提供采样器和数值参数：

```cpp
const std::array updates{
    granit::material_parameter_update::value(
        granit::material_parameter_id("base_color"),
        granit::material_parameter_type::float4,
        std::as_bytes(std::span{&base_color, 1})),
    granit::material_parameter_update::value(
        granit::material_parameter_id("metallic"),
        granit::material_parameter_type::float32,
        std::as_bytes(std::span{&metallic, 1})),
    granit::material_parameter_update::texture_binding(
        granit::material_parameter_id("normal_texture"), normal_view.ref()),
    granit::material_parameter_update::sampler_binding(
        granit::material_parameter_id("pbr_sampler"), sampler.ref()),
};

check(material.initialize(renderer, {
    .archive = material_archive,
    .initial_updates = updates,
    .shader_library = shader_library.ref(),
}));
```

归档和更新数据只需保持到 `initialize` 返回。Shader Library、Texture View 和 Sampler 是 Material
使用的 GPU 资源，销毁 Material 前必须保持有效。参数名通过稳定 Parameter ID 查找，类型或字节数
与 Schema 不一致时创建会失败。

## 4. 提交场景和方向光

Scene Snapshot 复制 View、Renderable 和 Light 数组。Renderable 的 `payload` 是上层关联键；同值的
Draw Binding 把它映射到真正的 Mesh 和 Material：

```cpp
const granit::render_pipeline_draw_binding binding{
    .payload = 1,
    .mesh = mesh.ref(),
    .material = material.ref(),
};

granit::render_pipeline_render_desc render_desc{};
render_desc.scene = scene.ref();
render_desc.output = backbuffer.view;
render_desc.output_format = swapchain_info.format;
render_desc.width = swapchain_info.width;
render_desc.height = swapchain_info.height;
render_desc.draw_bindings = std::span{&binding, 1};
render_desc.frame = &frame;
check(pipeline.render(render_desc));
```

本章只显式提供一盏白色方向光。Render Pipeline 使用内置默认 IBL 资源完成 PBR 必需的环境项，应用
暂时不加载 `.grenv`。Model Matrix 只有旋转，因此可同时作为 Normal Matrix；加入非均匀缩放后必须
改用 Model Matrix 左上 3×3 的逆转置。

## 5. 构建并运行

完整源码位于
[`examples/tutorials/06_material_and_lighting`](../../examples/tutorials/06_material_and_lighting)：

```powershell
cmake --preset windows-clang-debug -DGRANIT_SHADER_TOOLCHAIN_MODE=auto
cmake --build --preset windows-clang-debug `
  --target granit_tutorial_06_material_and_lighting
.\build\windows-clang-debug\bin\granit_tutorial_06_material_and_lighting.exe
```

自动验证会绘制三帧并强制走一次 Swapchain Recreate：

```powershell
ctest --preset windows-clang-debug `
  -R granit.tutorial.06_material_and_lighting --output-on-failure
```

## 6. 验收与生命周期

- 立方体显示暖色 PBR 材质，并随旋转呈现连续的直接光照变化。
- 修改 `base_color`、`metallic` 或 `perceptual_roughness` 后外观相应改变。
- Resize 后继续绘制，应用无需自行管理深度或 HDR 中间纹理。
- 退出时依次销毁 Scene、Render Pipeline、Material、Mesh、材质纹理和 Shader Library。

准确契约见 [Material](../reference/material.md)、[Scene Snapshot](../reference/scene-snapshot.md)、
[Render Pipeline](../reference/render-pipeline.md)和 [Shader Library](../reference/shader-library.md)。下一章
在当前最小提交路径上解释参考管线提供的完整帧行为。

[上一章：组织 Mesh](05-mesh.md) · [下一章：使用 Render Pipeline](07-render-pipeline.md)
