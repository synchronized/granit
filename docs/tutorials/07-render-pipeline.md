<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 07：理解 Render Pipeline

上一章已经用 Render Pipeline 提交一个最小 PBR 场景。本章保留相同资产流程，增加第二个 Material、
地面、可见阴影和完整帧边界，集中说明 Scene Snapshot、Draw Binding 与参考管线各自负责什么。

完成后，旋转立方体会在粗糙地面上投下阴影。应用只提供场景值、资源映射和最终输出；深度、Shadow、
HDR、Tone Mapping 与可选 FXAA 由 Render Pipeline 编排。

## 1. Scene Snapshot 只保存场景值

每帧创建 Snapshot 时传入 View、Renderable 和 Light。创建调用会复制这些数组，局部变量随后可以
释放。Snapshot 不拥有 Mesh 或 Material，也不会根据文件名查找 GPU 资源。

本章提交两个 Renderable：

- `payload = 1`：旋转立方体；
- `payload = 2`：非均匀缩放后的地面。

地面的 Normal Matrix 使用 Model Matrix 线性部分的逆转置。对于本章没有旋转的缩放矩阵，这等价于
把三个缩放量分别取倒数。直接把非均匀缩放的 Model Matrix 当作 Normal Matrix 会破坏光照方向。

## 2. Draw Binding 连接场景与 GPU 资源

`payload` 是上层分配的关联键，不是 Granit 句柄。Render Pipeline 通过同值的 Draw Binding 找到
真正的 Mesh 和 Material：

```cpp
const std::array bindings{
    granit::render_pipeline_draw_binding{
        .payload = 1, .mesh = mesh.ref(), .material = cube_material.ref()},
    granit::render_pipeline_draw_binding{
        .payload = 2, .mesh = mesh.ref(), .material = floor_material.ref()},
};
```

两个对象可以借用同一个 Mesh，同时使用不同的 Material。每个可见 Renderable 必须恰好有一个对应
Binding；重复 payload、无效资源或缺失映射会返回明确错误。

## 3. 参考管线执行完整帧

一次 `render` 调用会根据当前场景组织内部 Render Graph：

```text
Scene Snapshot
  → Visibility
  → Directional Shadow
  → Forward PBR（HDR + Depth）
  → Tone Mapping / FXAA
  → Swapchain Backbuffer
```

应用不创建这些中间 Texture，也不手工录制各 Pass。`render_pipeline_desc` 可以选择 Sample Count、
FXAA 和 Specular AA；示例同时尝试启用阶段 GPU Metrics，不支持 Timestamp Query 的后端会继续渲染。

Render Pipeline 记录并提交传入的 `acquired_frame`，成功后应用仍负责调用 `swapchain.present(frame)`。
失败时必须取消仍有效的 Frame，不能把未完成的 Acquire 留在飞行队列中。

## 4. 空场景仍然提交

没有可见物体时，仍然用包含 View 的空 Snapshot 调用 `render`，并传入空 Draw Binding：

```cpp
render_desc.scene = empty_scene.ref();
render_desc.draw_bindings = {};
check(pipeline.render(render_desc));
check(swapchain.present(frame));
```

这条路径会执行清屏、可选 Overlay、提交和 Present。上层不能因为可见列表为空而跳过整帧，否则只有
UI 的启动页、编辑器空场景或加载界面可能保持旧 Backbuffer，甚至阻塞后续 Acquire。

本章 Smoke 的第一帧专门验证空场景，后续帧再提交立方体、地面和方向光。

## 5. Resize 与内部资源

窗口尺寸变化或 Frame 报告需要重建时，应用重建 Swapchain 并更新 Viewport 尺寸。下一次 `render`
会按新的输出尺寸重新取得内部 HDR、Depth 和其他瞬时资源；应用不需要保存或重建这些对象。

输出格式和尺寸必须与当前 Backbuffer 匹配。过期 Backbuffer View 的借用期在 Present、Cancel 或
Swapchain Recreate 时结束，不能跨帧缓存。

## 6. 构建并运行

完整源码位于
[`examples/tutorials/07_render_pipeline`](../../examples/tutorials/07_render_pipeline)：

```powershell
cmake --preset windows-clang-debug -DGRANIT_SHADER_TOOLCHAIN_MODE=auto
cmake --build --preset windows-clang-debug `
  --target granit_tutorial_07_render_pipeline
.\build\windows-clang-debug\bin\granit_tutorial_07_render_pipeline.exe
```

自动验证包含空场景、正常 PBR 绘制和两次强制 Swapchain Recreate：

```powershell
ctest --preset windows-clang-debug `
  -R granit.tutorial.07_render_pipeline --output-on-failure
```

开发仓库还可以运行覆盖更多错误与资源路径的 Render Pipeline 集成测试：

```powershell
ctest --preset windows-clang-debug -R granit.pipeline.render --output-on-failure
```

## 7. 验收

- 立方体与地面使用不同材质，方向光在地面产生可见阴影。
- 第一帧没有 Renderable 时仍显示稳定清屏色并完成 Present。
- Resize 后阴影、HDR、深度和最终输出继续匹配窗口尺寸。
- Material、Mesh、Scene 或 Frame 来自其他 Renderer 时调用被拒绝。

输入和生命周期的准确契约见 [Render Pipeline](../reference/render-pipeline.md)、
[Scene Snapshot](../reference/scene-snapshot.md)和 [Material](../reference/material.md)。下一章在最终输出上
叠加 ImGui，并复用同一个 Window Loop 验证浏览器 WebGPU。

[上一章：材质与光照](06-material-and-lighting.md) · [下一章：接入 ImGui](08-imgui.md)
