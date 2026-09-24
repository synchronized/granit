<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 02：PBR 资产与参考渲染管线

本教程在第一个教程的应用生命周期之上改用 Granit Render Pipeline，展示 Shader Library、Material、
Scene Snapshot、阴影、HDR、Tone Mapping、FXAA 和 Canvas 合成。完整源码位于
[`examples/tutorials/02_pbr_assets`](../../examples/tutorials/02_pbr_assets)。

## 模型与构建期资产

教程输入是 Khronos glTF Sample Assets 的 CC0 Suzanne 模型、BIN、Base Color 与
Metallic/Roughness 纹理。两端都通过 `tutorials/02_pbr_assets/...` 逻辑路径读取；CMake 在桌面复制
运行时文件，并在浏览器预载同一组文件。来源、许可和摘要见
[`资产说明`](../../examples/assets/tutorials/02_pbr_assets/README.md)。

仓库私有 glTF 支持层解析模型和图片，GPU Scene 支持层将真实顶点、索引、纹理、Sampler 与 PBR
Material 上传到 Granit。Shader Library 与 Material Archive 仍来自 HLSL-first 的锁定资产快照，
运行时不调用 DXC 或 Tint。

## 场景与渲染

每帧创建只描述当前可见内容的 Scene Snapshot：

- 一个相机 View；
- 带法线、切线、UV 与两张 PBR 纹理的 Suzanne Renderable；
- 一盏投射阴影的方向光；
- Payload 到 Mesh/Material 的 Draw Binding。

随后把 Application 获取的 Frame 与 Backbuffer 交给 `render_pipeline::render`。参考管线完成可见性、
阴影、PBR 光照、HDR、Tone Mapping、可选 FXAA 和 Canvas 合成，Application 在成功后统一 Present。
自动 Smoke 的第一帧故意不提供 Renderable，用来覆盖没有可见物体时仍清屏、绘制 UI 并提交帧的路径。

ImGui 面板实时修改 Base Color、Metallic 和 Roughness，并显示模型实际使用的 Base Color 纹理；
参数更新直接写入 Material Instance。Canvas 与 3D 场景共享最终输出，因此可以同时验证 UI 方向、
裁剪、纹理和后处理后的合成结果。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_02_pbr_assets
build/windows-clang-debug/bin/granit_tutorial_02_pbr_assets.exe

ctest --preset windows-clang-debug `
  -R "^granit\.tutorial\.02_pbr_assets$" --output-on-failure
```

浏览器版本：

```powershell
cmake --preset emscripten-debug
cmake --build --preset emscripten-debug --target granit_tutorial_02_pbr_assets
npm --prefix tests/web run test:tutorial-02 -- ../../build/emscripten-debug/web
```

参考管线各阶段和资源所有权见 [Render Pipeline](../reference/render-pipeline.md)、
[Material](../reference/material.md) 与 [Scene](../reference/scene-snapshot.md)。任意模型、异步加载和
环境光工作流属于 [Model Viewer Sample](../guides/model-viewer.md)。
