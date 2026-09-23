<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 02：PBR 资产与参考渲染管线

本教程在第一个教程的应用生命周期之上改用 Granit Render Pipeline，展示 Shader Library、Material、
Scene Snapshot、阴影、HDR、Tone Mapping、FXAA 和 Canvas 合成。完整源码位于
[`examples/tutorials/02_pbr_assets`](../../examples/tutorials/02_pbr_assets)。

## 构建期资产

作者输入是 `assets/sources/shaders/pbr/pbr_standard.hlsl`、Shader Library Manifest 和
`assets/sources/materials/pbr_standard.grmat.json`。桌面构建通过 AssetTools 生成 `.grshlib` 与
`.grmat`；浏览器构建预载仓库锁定的相同资产快照。运行时不调用 DXC 或 Tint。

教程通过 Shader Library 的逻辑名称创建 Material 需要的 Shader，使 C++ 源码不依赖构建后才确定
的内容摘要。Material Archive 描述参数布局和 Shader 入口，Material Instance 保存颜色、金属度、
粗糙度和纹理绑定。

## 场景与渲染

每帧创建只描述当前可见内容的 Scene Snapshot：

- 一个相机 View；
- 带法线、切线和 UV 的 Sphere，以及由同一 Mesh 缩放得到的平台 Renderable；
- 一盏投射阴影的方向光；
- Payload 到 Mesh/Material 的 Draw Binding。

随后把 Application 获取的 Frame 与 Backbuffer 交给 `render_pipeline::render`。参考管线完成可见性、
阴影、PBR 光照、HDR、Tone Mapping、可选 FXAA 和 Canvas 合成，Application 在成功后统一 Present。
自动 Smoke 的第一帧故意不提供 Renderable，用来覆盖没有可见物体时仍清屏、绘制 UI 并提交帧的路径。

ImGui 面板实时修改 Base Color、Metallic 和 Roughness；参数更新直接写入 Material Instance。Canvas
与 3D 场景共享最终输出，因此可以同时验证 UI 方向、裁剪、纹理和后处理后的合成结果。

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
[Material](../reference/material.md) 与 [Scene](../reference/scene-snapshot.md)。完整 glTF 加载和环境光工作流属于
[Model Viewer Sample](../guides/model-viewer.md)。
