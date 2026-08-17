<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 示例程序

## 构建与运行

顶层构建默认启用 `GRANIT_BUILD_EXAMPLES`。单配置生成器将可执行文件和 Windows DLL 放在
`bin`；Visual Studio 等多配置生成器使用 `bin/<配置>`，因此共享库示例无需额外修改 `PATH`。

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug
build/windows-clang-debug/bin/granit_offscreen_clear_example.exe
```

Win32 窗口示例在 Windows 构建；检测到 XCB 开发包时，Linux 额外构建 XCB 清屏示例。更多构建
选项见[构建与安装](build.md)。

## 公共接口示例

- `granit_version_example`：查询版本。
- `granit_renderer_example`：创建和销毁 Renderer。
- `granit_offscreen_clear_example`：创建离屏颜色附件并清屏。
- `granit_texture_readback_example`：离屏清屏、同步回读并校验原始 RGBA8 像素；传入路径时
  额外写出 `.rgba` 文件。接口语义见[纹理同步回读](texture-readback.md)。
- `granit_offscreen_triangle_example`：使用预编译 SPIR-V 绘制最小三角形。
- `granit_compute_example`：Compute Shader 写入 Storage Buffer，并复制回读结果。
- `granit_window_clear_example`：使用 `granit::window` 执行 Win32 事件、acquire、清屏、submit、
  present 与尺寸重建循环；`--smoke-test` 渲染三帧后自动退出。
- `granit_window_triangle_example`：上传顶点数据并持续绘制窗口三角形。
- `granit_xcb_window_clear_example`：在 Linux XCB 窗口中完成清屏、Present 和 Resize 重建；使用
  `--smoke-test` 时渲染三帧后自动退出。
- `granit_wayland_window_clear_example`：使用 `xdg-shell` 创建 Wayland 顶层窗口，处理 configure、
  清屏、Present 和 Resize 重建；使用 `--smoke-test` 时渲染三帧后自动退出。
- `granit_render_pipeline_offscreen_example`：通过公共 Mesh、Material、Scene 与 Render Pipeline API
  自动录制 Shadow/Opaque Draw，并回读输出像素。完整流程见
  [Render Pipeline 离屏渲染教程](../tutorials/render-pipeline-offscreen.md)。
- `granit_render_pipeline_window_example`：把 Swapchain Frame 和 Backbuffer 交给公共 Render
  Pipeline，并处理帧同步与窗口重建。
- `granit_immediate_ui_adapter_example`：把仿第三方立即式 UI Draw Data 转换为公共 Canvas
  顶点、索引、借用状态和 Scissor；不依赖具体 UI 库。
- `granit_sdl3_window_clear_example`：启用 SDL3 Integration 时构建；由 SDL3 创建窗口和处理事件，
  通过适配组件创建 Granit Surface，并完成 Swapchain 清屏、Present 和像素尺寸变化重建。

## 内部联调示例

- `granit_material_hot_reload_example`：演示错误材质回退和成功热替换。
- `granit_pbr_offscreen_example`：验证带深度的离屏 PBR Draw、默认纹理和像素回归。
- `granit_window_hdr_example`：验证 PBR、HDR、Tone Mapping 和窗口输出组合。

这些示例会直接使用尚未安装的 `granit::material` 或 `granit::lighting` 开发模块，不应作为外部
Consumer 的稳定集成方式。

## 资源归属

```text
src/pipeline/shaders/   正式 Pipeline 内置 Shader
assets/shaders/pbr/     示例、测试和工具共享的 PBR 参考 Shader
examples/assets/        仅属于单个示例的资源
```

正式库源码不得反向依赖 `examples` 或 `tests` 目录。示例不包含 Vulkan 头文件；预编译 SPIR-V
随仓库提供，常规示例构建不要求运行时 Shader 编译器。
