<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 使用 Render Pipeline 完成第一次离屏渲染

本教程通过仓库自带 Smoke 程序渲染一个 64×64 的 PBR 三角形，并回读中心像素验证结果。完成后可以理解
Renderer、Mesh、Material、Scene Snapshot 和 Render Pipeline 如何组成一次完整渲染。

## 前置条件

- 已安装 CMake、支持 C++20 的编译器和对应生成器。
- 系统具有可用的 Vulkan loader、显卡驱动和 Vulkan 1.3 设备。
- 使用包含 `RenderPipeline` component 的仓库构建。

构建环境和安装方式见[构建与安装](../guides/build.md)。

## 1. 构建并运行 Smoke

Windows Clang + Ninja：

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
build/windows-clang-debug/bin/granit_render_pipeline_offscreen_smoke.exe
```

成功时程序输出如下形式的消息，具体像素值可能因设备而异：

```text
Render Pipeline 离屏渲染成功，中心像素：R, G, B
```

也可以通过 CTest 单独运行：

```powershell
ctest --preset windows-clang-debug -R "^granit.smoke.render_pipeline_offscreen$"
```

## 2. 创建 Renderer 和输出目标

示例先创建 `granit::renderer`，再创建一个具有以下属性的 64×64 Texture：

- 格式为 `GRANIT_TEXTURE_FORMAT_RGBA8_UNORM`。
- usage 同时包含 Color Attachment 和 Transfer Source。
- 创建默认 Texture View，作为 Render Pipeline 的 LDR 输出。

Transfer Source usage 是后续像素回读所必需的，并非普通窗口渲染的固定要求。

## 3. 准备 Mesh

示例上传三个三维位置到 device-local Vertex Buffer，然后通过 `granit_mesh_desc` 声明：

- 一个 stride 为 12 字节的 Vertex Buffer。
- location 0 使用 `GRANIT_VERTEX_FORMAT_FLOAT32X3`。
- 三个顶点，采用默认 Triangle List 拓扑。

Mesh 复制顶点布局和绘制范围，但只借用 Vertex Buffer。因此 Vertex Buffer 必须比 Mesh 活得更久。
完整约束见 [Mesh 参考](../reference/mesh.md)。

## 4. 创建 Material

CMake 在构建 Smoke 时使用 `granit_material_tool`，把
`tests/fixtures/smoke/render_pipeline_untextured.grmat.json` 编译为材质归档。程序读取归档，并在创建
Material 时把 `base_color` 初始化为红色系颜色。

参数名先通过 `granit_material_parameter_id` 转换为稳定 ID，再与类型、数据和尺寸一起提交。
归档字节只需在创建调用期间有效。格式和更新规则见 [Material 参考](../reference/material.md)。

## 5. 生成 Scene Snapshot

示例建立一个最小场景：

- 一个使用单位矩阵和完整 layer mask 的 View。
- 一个包围球半径为 1、`payload` 为 1 的 Renderable。
- 一个朝向场景的白色方向光。

Scene Snapshot 在创建时复制这些值数据，因此局部数组之后可以释放。`payload` 只是上层分配的关联
键，下一步用它查找真正的 Mesh 和 Material。详见
[Scene Snapshot 参考](../reference/scene-snapshot.md)。

## 6. 绑定并执行 Render Pipeline

示例创建默认 Render Pipeline，不提供阶段回调，因此使用自动 Shadow 和 Opaque Draw 路径。随后
建立唯一的 Draw Binding：

```text
Renderable payload 1 -> Mesh + Material
```

单 View 渲染描述提供 Scene Snapshot、输出 Texture View、格式、尺寸和 Draw Binding。调用
`granit_render_pipeline_render` 后，参考管线依次完成：

```text
Scene Snapshot
  -> 可见性与 Draw 匹配
  -> Directional Shadow
  -> Forward PBR HDR
  -> ACES Tone Mapping
  -> RGBA8 离屏输出
```

输入、扩展回调和并发限制见 [Render Pipeline 参考](../reference/render-pipeline.md)。

## 7. 回读并验证像素

Render Pipeline 已经完成离屏执行。示例另外创建 readback Buffer 和 Command Recorder，把输出
Texture 复制到 Buffer，然后映射 CPU 内存并检查中心像素。任一 RGB 通道非零即说明三角形覆盖了
中心位置。

回读是 Smoke 的自动验证步骤，不是 Render Pipeline 的必需阶段。实际应用可以继续采样输出 Texture、
传递给后处理，或改用 Swapchain Frame 进行窗口显示。

## 8. 按所有权顺序销毁

示例结束时按依赖关系释放：

```text
Render Pipeline
Scene Snapshot
Material
Mesh
Output Texture View
Output Texture
Vertex Buffer 与 Renderer 由 C++ RAII 包装随后释放
```

关键原则是：销毁借用方之后再销毁被借用资源，并且不得让销毁与渲染并发。

## 下一步

- 阅读[示例程序说明](../guides/examples.md)，了解面向使用者的完整应用示例。
- 使用 [Material 参数更新](../reference/material.md)在帧间修改外观。
- 使用[录制回调](../reference/render-pipeline.md#录制回调)替换 Shadow 或 Opaque Draw。
- 绕过参考管线，直接学习 [Command Recorder](../reference/command-recorder.md)。

本教程对应的完整源码位于
[`tests/smoke/render_pipeline_offscreen.cpp`](../../tests/smoke/render_pipeline_offscreen.cpp)。
