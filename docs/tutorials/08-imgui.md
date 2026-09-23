<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 08：接入 ImGui 调试界面

本章在上一章的 PBR 场景上叠加 ImGui。窗口与输入仍由 `granit::window` 提供，ImGui 只负责生成
界面；Draw Data 经 Granit ImGui Integration 转换为 Canvas，再由 Render Pipeline 合成到最终输出。

完成后可以用面板实时修改立方体的固有色、金属度和粗糙度，并显示一张自定义 Texture ID 图片。
同一份应用代码可以运行在桌面 Vulkan 和浏览器 WebGPU 上。

## 1. 初始化 ImGui 与 GPU 资源

Renderer 就绪后创建 ImGui Context，并关闭本教程不需要的磁盘布局文件。随后创建 Canvas、字体纹理、
字体采样器和一张演示自定义 Texture ID 的棋盘纹理：

```cpp
IMGUI_CHECKVERSION();
ImGui::CreateContext();
ImGui::GetIO().IniFilename = nullptr;

check(canvas.initialize(renderer));
check(tutorial_imgui::upload_font_atlas(renderer, font_texture, font_view, font_sampler));
check(tutorial_imgui::upload_checker(renderer, checker_texture, checker_view));
```

这些 GPU 对象由应用拥有。Texture ID 只是适配层使用的稳定键，不是原生 Vulkan、WebGPU 或 Granit
资源句柄。

## 2. 把统一事件送入 ImGui

每次 Tick 先轮询 Window 与 Input 事件。`imgui_input.cpp` 把 Granit 的键盘、文本、指针、滚轮和焦点
事件转换为 `ImGuiIO::Add*Event` 调用，不读取 Win32、XCB、Wayland 或 DOM 对象：

```cpp
while (window_system.poll(event).ok())
  tutorial_imgui::process_input_event(event);

tutorial_imgui::begin_frame(window_state, delta_seconds);
ImGui::NewFrame();
```

`begin_frame` 同时更新逻辑显示尺寸、Framebuffer Scale 和 Delta Time。桌面与 Emscripten 因而复用
同一个 Tick 和界面代码；平台适配只存在于 Window 模块内部。

## 3. 用面板更新 Material

面板直接编辑应用保存的材质值。值发生变化时，使用上一章相同的稳定 Parameter ID 更新 Material：

```cpp
bool changed = ImGui::ColorEdit3("Base color", &base_color.x);
changed = ImGui::SliderFloat("Metallic", &metallic, 0.0F, 1.0F) || changed;
changed = ImGui::SliderFloat("Roughness", &roughness, 0.04F, 1.0F) || changed;

if (changed)
  check(update_material());
```

ImGui 不保存 Material，也不直接访问 Uniform Buffer。应用仍拥有参数值和 Material 生命周期。

## 4. 解析 Font Atlas 与自定义 Texture ID

示例为字体和棋盘图分配两个应用级 ID。转换 Draw Data 时，Resolver 把 ID 映射为借用的 Texture View
和 Sampler：

```cpp
ImGui::Image(ImTextureRef{tutorial_imgui::checker_texture_id}, {64, 64});

check(granit::integration::imgui::append_draw_data(
    ImGui::GetDrawData(), canvas,
    tutorial_imgui::resolve_texture, &texture_bindings));
```

Resolver 只在转换调用期间借用注册表。未知或已失效 ID 会返回错误，不能把已销毁资源继续交给 Canvas。

## 5. 将 Canvas 叠加到最终输出

每帧先清空 Canvas，再转换当前 ImGui Draw Data。Canvas 会保存转换后的顶点、索引、Scissor 和纹理
绑定；Render Pipeline 在 Tone Mapping 后将它合成到 Backbuffer：

```cpp
check(canvas.clear());
check(granit::integration::imgui::append_draw_data(
    ImGui::GetDrawData(), canvas,
    tutorial_imgui::resolve_texture, &texture_bindings));

granit::render_pipeline_render_desc desc{};
desc.scene = scene.ref();
desc.output = backbuffer.view;
desc.output_format = swapchain_info.format;
desc.width = swapchain_info.width;
desc.height = swapchain_info.height;
desc.canvas = canvas.ref();
check(pipeline.render(desc));
```

输入、界面构建和渲染的完整帧顺序是：

```text
process events → begin ImGui frame → build UI → ImGui::Render
  → append Draw Data to Canvas → acquire → Render Pipeline → present
```

Resize 后先重建 Swapchain，再用新尺寸更新 ImGui Framebuffer Scale 和 Render Pipeline 输出。

## 6. 构建并运行

完整源码位于 [`examples/tutorials/08_imgui`](../../examples/tutorials/08_imgui)。桌面构建与 Smoke：

```powershell
cmake --preset windows-clang-debug -DGRANIT_SHADER_TOOLCHAIN_MODE=auto
cmake --build --preset windows-clang-debug --target granit_tutorial_08_imgui
.\build\windows-clang-debug\bin\granit_tutorial_08_imgui.exe

ctest --preset windows-clang-debug -R granit.tutorial.08_imgui --output-on-failure
```

浏览器构建会预加载已生成的 Shader Library 与 Material，并运行相同的教程代码：

```powershell
cmake --preset emscripten-debug
cmake --build --preset emscripten-debug --target granit_tutorial_08_imgui
```

仓库的 `browser-tutorial-08` 工作流验证 WebGPU 多帧渲染、Canvas 内容、指针输入和 Resize。浏览器
环境准备与手工启动方式见[浏览器 WebGPU 指南](../guides/webgpu-browser-example.md)。

## 7. 生命周期与验收

关闭时先停止 Tick，再依次释放 Scene、Render Pipeline、Canvas、Material、Mesh、UI 纹理与 Shader
Library，最后销毁 ImGui Context、Swapchain、Renderer 和 Window。这样不会在 Renderer 销毁时留下
Shader Library 或 Canvas 资源。

- 面板能修改立方体的固有色、金属度和粗糙度。
- Font Atlas 与自定义棋盘 Texture ID 都正确显示。
- 指针、按钮、滚轮、键盘、文本和焦点通过统一事件路径进入 ImGui。
- Resize 后场景、裁剪和点击位置继续匹配窗口尺寸。
- Vulkan 与浏览器 WebGPU 都能持续提交包含 Canvas 的帧。
- 关闭时没有 Draw Data 转换失败或用户资源残留诊断。

[上一章：使用 Render Pipeline](07-render-pipeline.md) · [下一章：加载 glTF 模型](09-model-loading.md)
