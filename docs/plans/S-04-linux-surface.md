<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-04：Linux XCB 与 Wayland Surface

## 状态

- 设计状态：已确认
- 实现状态：实现中
- 路线图任务：S-04
- 优先级：P2
- 前置依赖：现有 Win32 Surface、Swapchain、S-03

## 背景与目标

Granit 当前只提供 Win32 原生 Surface。Linux 无窗口渲染可以工作，但窗口输出尚不能从 X11 或
Wayland 原生窗口创建 Vulkan Surface，也无法让 SDL、GLFW 和上层引擎在 Linux 上接入 Swapchain。

S-04 在不公开 Vulkan 类型、不接管窗口与事件循环的前提下增加 Linux 平台 Surface：

- 第一阶段支持 XCB，先打通 Linux 窗口、Surface、Swapchain 和 Present 全链路。
- 第二阶段支持 Wayland，保持与 XCB 相同的公共生命周期和错误语义。
- 公共 C ABI 只接收原生窗口系统值，不要求使用者包含 XCB、Wayland 或 Vulkan 头文件。
- SDL、GLFW 等窗口库负责创建窗口、事件循环和输入，Granit 只借用创建 Surface 所需值。

## 非目标

- 不实现窗口创建、标题、尺寸、输入、剪贴板、拖放或事件循环。
- 不实现 Wayland `xdg-shell`、窗口装饰和输入法协议。
- 不在 Granit 内部依赖 SDL、GLFW 或其他完整窗口框架。
- 不增加 Xlib 路径；X11 首版统一使用 XCB。
- 不向普通用户公开 `VkSurfaceKHR` 或平台 Vulkan 扩展结构。

## 公共接口决策

### Renderer 能力声明

`granit_renderer_desc::surface_types` 增加两个稳定标志：

- `GRANIT_SURFACE_TYPE_XCB_BIT`
- `GRANIT_SURFACE_TYPE_WAYLAND_BIT`

Renderer 创建时只启用调用方声明的平台扩展。请求当前构建不支持或 Vulkan loader 未提供的扩展
时返回 `GRANIT_ERROR_UNSUPPORTED`，不延迟到 Surface 创建时静默失败。

### XCB 描述

XCB 描述包含：

- `void* connection`：调用期间借用的 `xcb_connection_t*`。
- `uint32_t window`：按 XCB 协议定义保存 `xcb_window_t`。
- `struct_size`：保留尾部扩展能力。

公共头不包含 `<xcb/xcb.h>`。调用方必须保证 connection 与 window 有效，并在 Granit Surface
销毁前维持对应 X11 连接和窗口生命周期。

### Wayland 描述

Wayland 描述包含调用期间借用的 `void* display` 和 `void* surface`，分别对应 `wl_display*` 与
`wl_surface*`。公共头不包含 Wayland 头文件；Display 和 Surface 的所有权始终属于调用方。

### C++20 包装

现有 move-only `granit::surface` 增加 `initialize_xcb` 和 `initialize_wayland`，继续复用同一个
Renderer/Surface 句柄与 RAII 销毁路径，不创建平台专用运行时对象层级。

## 构建与依赖边界

- Linux 构建分别探测 XCB 和 Wayland 客户端开发包；找到依赖时定义对应 Vulkan 平台宏。
- 平台头文件和链接依赖只属于 Granit 私有目标，不传播到安装包或 Consumer。
- 公共 API 声明始终可由 C11 编译；某平台后端未编译时，对应创建入口返回不支持。
- Linux CI 安装必要开发包，并继续运行安装导出审计，防止 XCB、Wayland 和 Vulkan 依赖泄漏。
- Windows 构建继续只启用 Win32 Vulkan 平台声明，不要求安装 Linux 平台头文件。

## 实施顺序

1. S-04A（已完成）：已增加 XCB/Wayland 能力位、C/C++ 描述和入口，并建立 ABI 布局、
   公共导出和参数验证测试；后端接入前有效请求明确返回不支持。
2. S-04B（已完成）：Linux 构建私有探测 XCB 头文件，Instance 按请求启用 XCB 扩展，并已
   实现真实 XCB Surface 创建、呈现队列校验和 Registry 生命周期接入；本机 Windows 条件关闭
   路径已验证，Linux 条件编译等待受账户额度限制的远端 CI 恢复后重跑。
3. S-04C（实现完成，等待远端复测）：已增加基于 Xvfb 的真实 XCB 窗口、Swapchain、附件清屏和
   Present 集成测试，并提供处理关闭、Resize 与重建的 XCB 清屏示例；Linux CI 已准备 Xvfb、
   Xauth 与 Mesa 软件 Vulkan 环境，等待账户额度恢复后执行远端验收。
4. S-04D（实现完成，等待远端复测）：已增加私有 Wayland Client 探测、Instance 扩展启用、
   真实 Wayland Surface 创建、呈现队列校验和 Registry 生命周期接入；Linux 条件编译与运行等待
   账户额度恢复后执行远端验收。
5. S-04E（实现中）：已配置 Weston Headless 和私有 `xdg-shell` 协议生成，并增加真实 Wayland
   顶层窗口、Swapchain、附件清屏和 Present 集成测试；Resize、窗口示例及 SDL/GLFW 指南待完成。

## 测试与验收

- C11 头文件不需要 XCB、Wayland 或 Vulkan 头文件即可独立编译。
- 能力位、描述大小、字段偏移和公开导出符号进入 S-01 回归。
- 未声明能力、空原生值、错误平台、跨 Renderer 与重复销毁均返回稳定结果。
- XCB 和 Wayland 分别创建真实 Surface、Swapchain，至少完成清屏、Present 与 Resize 重建。
- 关闭某个平台构建能力时，公共入口仍存在并稳定返回 `GRANIT_ERROR_UNSUPPORTED`。
- 安装导出不包含平台窗口系统或 Vulkan 的传递 include、链接库和编译定义。

## 风险与未决问题

- Vulkan 的 XCB Presentation Support 查询需要 visual ID；首版在真实 Surface 创建后以
  `vkGetPhysicalDeviceSurfaceSupportKHR` 为最终判定，不把 visual ID 放入公共 ABI。
- Wayland Surface 的窗口角色由调用方和 `xdg-shell` 管理；角色尚未建立时创建或呈现可能失败。
- CI 的 Xvfb、Weston 等运行环境需要与 Vulkan 软件驱动同时验证，不能只测试窗口连接成功。
- SDL2、SDL3 与 GLFW 暴露原生窗口信息的接口不同，适配文档必须按版本分别说明。
