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

窗口示例目前仅在 Windows 构建。更多构建选项见[构建与安装](build.md)。

## 公共接口示例

- `granit_version_example`：查询版本。
- `granit_renderer_example`：创建和销毁 Renderer。
- `granit_offscreen_clear_example`：创建离屏颜色附件并清屏。
- `granit_offscreen_triangle_example`：使用预编译 SPIR-V 绘制最小三角形。
- `granit_compute_example`：Compute Shader 写入 Storage Buffer，并复制回读结果。
- `granit_window_clear_example`：执行 Win32 acquire、清屏、submit、present 与尺寸重建循环。
- `granit_window_triangle_example`：上传顶点数据并持续绘制窗口三角形。
- `granit_render_pipeline_offscreen_example`：通过公共 Mesh、Material、Scene 与 Render Pipeline API
  自动录制 Shadow/Opaque Draw，并回读输出像素。
- `granit_render_pipeline_window_example`：把 Swapchain Frame 和 Backbuffer 交给公共 Render
  Pipeline，并处理帧同步与窗口重建。

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
