<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从窗口到完整渲染程序

本系列从一个空的 CMake C++20 程序开始，每章在上一章基础上增加一种可见能力。读者不需要预先
了解 Vulkan、WebGPU 或平台原生窗口 API；教程只使用 Granit 公共 C++ 接口和 HLSL-first 资产流程。

01～10 均已提供配套源码和自动验证。01、08 与 10 包含浏览器 WebGPU 验证；09 提供同步桌面
模型加载，10 完成跨后端应用。当前接口行为以各章链接的 Reference 为准。

## 学习路线

| 章节 | 新增能力 | 完成后的可见结果 | 配套源码 |
|---|---|---|---|
| [01：Window](01-window.md) | Renderer、Window、Surface、Swapchain、帧循环 | 可缩放的纯色窗口 | [01_window](../../examples/tutorials/01_window) |
| [02：Triangle](02-triangle.md) | Shader Library、Graphics Pipeline、Draw | 窗口中央三角形 | [02_triangle](../../examples/tutorials/02_triangle) |
| [03：Texture](03-texture.md) | Texture、Sampler、Bind Group、上传 | 带棋盘纹理的图形 | [03_texture](../../examples/tutorials/03_texture) |
| [04：Depth and Camera](04-depth-and-camera.md) | 顶点/索引、深度、Uniform、相机 | 可观察的旋转立方体 | [04_depth_and_camera](../../examples/tutorials/04_depth_and_camera) |
| [05：Mesh](05-mesh.md) | Mesh 与最小模型加载 | 由模型数据驱动的物体 | [05_mesh](../../examples/tutorials/05_mesh) |
| [06：Material and Lighting](06-material-and-lighting.md) | `.grmat`、方向光、PBR | 受光照影响的材质立方体 | [06_material_and_lighting](../../examples/tutorials/06_material_and_lighting) |
| [07：Render Pipeline](07-render-pipeline.md) | Scene、Draw Binding、Shadow、HDR | 参考管线渲染场景 | [07_render_pipeline](../../examples/tutorials/07_render_pipeline) |
| [08：ImGui](08-imgui.md) | 输入、Canvas、Texture ID、调试面板 | 可交互的渲染工具界面 | [08_imgui](../../examples/tutorials/08_imgui) |
| [09：Model Loading](09-model-loading.md) | glTF/GLB、外部资源、GPU Scene | 加载真实 PBR 模型 | [09_model_loading](../../examples/tutorials/09_model_loading) |
| [10：Model Viewer](10-model-viewer.md) | 异步加载、环境光、轨道相机、检查器 | 跨后端 PBR 工具 | [10_model_viewer](../../examples/tutorials/10_model_viewer) |

## 使用方式

按编号阅读。每章只解释相对上一章增加的概念，并给出关键代码、运行结果和排错入口。配套完整源码
按相同编号进入 `examples/tutorials/<step>`；独立综合示例位于 `examples/samples`，测试和错误路径
位于 `tests`。Model Viewer 作为系列最终项目位于 Tutorial 10，不再维护重复 Sample。

各章采用相同阅读顺序：先确认本章新增能力，再阅读关键实现，按“构建并运行”执行配套源码，最后
根据验收项检查画面和生命周期。正文只保留理解当前增量需要的片段；遇到省略内容时直接查看表中的
完整源码，不从 Markdown 拼接程序。

桌面章节首先验证 Windows 与 Linux Vulkan。最后一章复用相同应用内容验证浏览器 WebGPU，平台差异
只存在于启动和事件循环壳层。构建、安装和依赖策略见[构建指南](../guides/build.md)。

## 系列约定

- 示例中的 `check(...)` 表示检查 `granit::result`，失败时打印 `message()` 并停止当前操作。
- 父对象必须比子资源存活更久，章节末尾会列出新增资源的销毁顺序。
- Shader 在构建期由 AssetTools 从 HLSL 生成 Shader Library，运行时不调用 DXC 或 Tint。
- 02 之后的 Shader 章节需要完整 Shader Toolchain；系统缺少 DXC 或 Tint 时，使用
  `-DGRANIT_SHADER_TOOLCHAIN_MODE=auto` 配置可下载项目锁定版本。
- 窗口和 Surface 使用 `granit::window`，不在教程中展开 Win32、XCB、Wayland 原生值。
- 每章完成后先验证预期画面，再进入下一章；不要一次复制最终应用跳过中间状态。
