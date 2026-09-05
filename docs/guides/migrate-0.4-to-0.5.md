<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.4 迁移到 0.5

## 适用范围

本文记录 0.5.0 相对 0.4.0 的源码迁移。0.5.0 已发布；升级后必须重新编译应用和
所有 Granit component，不能继续加载 0.4.0 动态库。

## C++ Result 判断

`granit::result` 已从枚举改为轻量值结构。结果常量名称保持不变，但原有 `succeeded()` 和
`failed()` 自由函数已经删除。

旧写法：

```cpp
const auto result = renderer.create(desc);
if (granit::failed(result))
  return result;
```

0.5 写法：

```cpp
const auto result = renderer.create(desc);
if (result.failed())
  return result;
```

成功判断使用 `result.ok()` 或显式布尔上下文；`true` 表示成功。需要访问 C ABI 值时使用
`result.native()`，错误文本使用 `result.message()`。C API 的 `granit_result` 未改变。

## 描述结构大小宏

项目尚未发布的中间结构版本宏已经收敛为当前大小宏。应用应使用对应的 `*_INIT` 初始化宏；只有
需要指定 `struct_size` 的代码才直接使用当前 `*_SIZE`：

| 0.4 名称 | 0.5 名称 |
|---|---|
| `GRANIT_RENDERER_DESC_VERSION_5_SIZE` | `GRANIT_RENDERER_DESC_SIZE` |
| `GRANIT_SHADER_DESC_VERSION_2_SIZE` | `GRANIT_SHADER_DESC_SIZE` |
| `GRANIT_GRAPHICS_PIPELINE_DESC_VERSION_5_SIZE` | `GRANIT_GRAPHICS_PIPELINE_DESC_SIZE` |
| `GRANIT_SWAPCHAIN_INFO_VERSION_2_SIZE` | `GRANIT_SWAPCHAIN_INFO_SIZE` |

更早的 `*_VERSION_N_SIZE` 名称也不再提供。不要手写结构大小，也不要用 Granit 包版本推断结构
能力。

## 桌面 WebGPU

桌面 Dawn WebGPU Provider 和动态插件边界已经删除。桌面应用使用 Vulkan；浏览器 WebGPU 继续由
Emscripten 静态后端提供。因此需要进行以下调整：

- 删除桌面 Dawn SDK 包和 Provider 插件的构建、部署及运行时搜索路径。
- 删除 `granit_renderer_desc` 的 `backend_library_path_length` 和 `backend_library_path` 赋值。
- 桌面选择 `GRANIT_RENDERER_BACKEND_VULKAN` 或 `GRANIT_RENDERER_BACKEND_AUTO`。
- Emscripten 目标仍可选择 `GRANIT_RENDERER_BACKEND_WEBGPU`。

## 窗口当前状态

新增的 `granit_window_get_state` 可在首个 Resize 或 Scale 事件到达前读取最近已知的逻辑尺寸、
Framebuffer 尺寸和内容缩放。它是兼容新增接口，不要求已有窗口循环迁移。调用必须发生在创建
Window System 的线程，详细契约见[Window component](../reference/window.md)。

## 验证升级

迁移后至少执行以下检查：

1. 清理旧的 CMake 配置和 0.4.0 动态库，使用同一套 0.5.0 头文件与库重新构建。
2. 编译所有 C11/C++20 Consumer，确认没有遗留 Result 自由函数和旧结构大小宏。
3. 桌面运行 Vulkan Smoke；浏览器目标运行 Emscripten WebGPU Smoke。
4. 验证窗口初始尺寸、缩放事件和关闭时资源归零。
