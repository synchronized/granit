<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 08：SDL3 + ImGui

本教程演示第三方框架拥有窗口和输入循环时，如何接入 Granit。SDL3 创建并持有 Window，ImGui
官方 SDL3 Platform Backend 接收事件；Granit 根据该 Window 创建 Surface，并通过 ImGui
Integration 把 Draw Data 转换为 Canvas 命令。完整源码位于
[`examples/tutorials/08_sdl_imgui`](../../examples/tutorials/08_sdl_imgui)。

## 所有权边界

```text
应用
├─ SDL_Window、事件循环与 ImGui Context
├─ 字体和自定义纹理的 Granit 资源
└─ Texture ID → Texture View/Sampler 映射
          ↓
Granit SDL3 Integration → Surface
Granit ImGui Integration → Canvas Draw List
          ↓
Swapchain → Frame Context → Submit → Present
```

Integration 不接管 SDL Window、ImGui Context、字体 Atlas 或 Texture ID。关闭时先结束 GPU 资源和
Renderer，再销毁 ImGui Context 与 SDL Window，避免动态库边界两侧产生悬空资源。

## 每帧流程

应用先把 SDL Event 交给 `ImGui_ImplSDL3_ProcessEvent`，随后执行 `NewFrame` 和界面构建。
`append_draw_data` 将 ImGui 顶点、索引、裁剪范围和 Texture ID 转换进 `canvas_draw_list`：

```cpp
result = granit::integration::imgui::append_draw_data(
    ImGui::GetDrawData(), canvas, resolve_texture, &bindings);
```

之后使用普通 Granit 帧流程录制 Canvas：

```text
acquire → frame_context.begin → canvas.record → submit → present
```

Window 像素尺寸变化时重建 Swapchain。逻辑窗口尺寸和像素尺寸不能混用，否则高 DPI 环境中的
裁剪区域与 Framebuffer Scale 会不一致。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_08_sdl_imgui
ctest --preset windows-clang-debug -R "^granit\.tutorial\.08_sdl_imgui$" --output-on-failure
```

Emscripten 使用同一界面与资源映射，入口按浏览器协作式主循环推进：

```powershell
cmake --preset emscripten-release
cmake --build --preset emscripten-release --target granit_tutorial_08_sdl_imgui
python -m http.server 8000 --directory build/emscripten-release/web
```

浏览器打开 `http://localhost:8000/granit_tutorial_08_sdl_imgui.html`。固定画面的 Vulkan 与浏览器
验收位于 `tests/integrations/imgui` 和 `tests/web/imgui`，不会把测试控制放进正常教程界面。
