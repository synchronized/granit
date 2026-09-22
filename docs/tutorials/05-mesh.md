<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 05：把几何组织为 Mesh

本章把上一章散落的 Buffer、Vertex Layout 和 Draw Range 收敛为 Mesh，并把仓库内的最小模型数据
与应用逻辑分离。完成后，渲染循环不再直接依赖某个模型的顶点数量和布局细节。通用 glTF 导入属于
综合 Model Viewer，不在本章引入第三方解析依赖。

## 1. 分离 CPU 数据与 GPU 资源

模型加载阶段产生顶点、索引和子网格范围；上传阶段创建 GPU Buffer；Mesh 创建阶段只描述这些
Buffer 如何解释和绘制。CPU 数组在上传完成后可以释放，GPU Buffer 必须比 Mesh 存活更久。

## 2. 创建 Mesh

Mesh 描述包含：

- 一个或多个 Vertex Buffer 及 stride；
- attribute location、offset 和 format；
- 可选 Index Buffer 与索引类型；
- topology、first index、vertex offset 和 draw count。

保持 Shader location 与 Mesh attribute 一致。模型缺少法线或 UV 时，应在导入阶段给出明确诊断，
而不是让 Shader 读取未定义数据。

```cpp
const granit::mesh_vertex_buffer binding{
    .buffer = vertex_buffer.ref(),
    .layout = {.stride = sizeof(model::vertex), .attributes = attributes},
};
granit::mesh mesh;
check(mesh.initialize(renderer, {
    .vertex_buffers = std::span{&binding, 1},
    .index_buffer = index_buffer.ref(),
    .index_format = granit::index_type::uint16,
    .index_count = static_cast<std::uint32_t>(model::indices.size()),
}));
```

## 3. 替换手工 Draw

帧循环继续负责附件和 Pipeline；在 Rendering 前调用 `mesh.bind(recorder)` 绑定并准备 Buffer，进入
Rendering 后调用 `mesh.draw(recorder)` 录制 Mesh 保存的 Draw Range。相机、深度和纹理绑定保持
不变，这也是资源对象化的价值：场景内容改变时不需要重写帧生命周期。

## 4. 构建并运行

完整源码位于 [`examples/tutorials/05_mesh`](../../examples/tutorials/05_mesh)，最小模型数据单独保存
在 [`model_data.hpp`](../../examples/tutorials/05_mesh/model_data.hpp)：

```powershell
cmake --preset windows-clang-debug -DGRANIT_SHADER_TOOLCHAIN_MODE=auto
cmake --build --preset windows-clang-debug --target granit_tutorial_05_mesh
.\build\windows-clang-debug\bin\granit_tutorial_05_mesh.exe
```

自动验证命令：

```powershell
ctest --preset windows-clang-debug -R granit.tutorial.05_mesh --output-on-failure
```

## 5. 验收

- 使用独立模型数据后画面与上一章具有相同的相机和深度行为。
- 销毁 CPU 模型数据不会影响已经完成上传的 Mesh。
- 先销毁 Mesh，再销毁它借用的 GPU Buffer。
- 非法 attribute 或越界 draw range 在创建阶段被拒绝。

本章开始链接 `granit::render_pipeline` component，因为公共 Mesh 位于该组件。完整程序见
[`main.cpp`](../../examples/tutorials/05_mesh/main.cpp)。

准确契约见 [Mesh](../reference/mesh.md)。下一章用材质资产替换手写纹理绑定，并加入光照。

[上一章：深度与相机](04-depth-and-camera.md) ·
[下一章：材质与光照](06-material-and-lighting.md)
