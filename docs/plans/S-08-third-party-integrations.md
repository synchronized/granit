<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-08：SDL3 与 ImGui 第三方集成

## 状态

- 设计状态：已确认
- 实现状态：S-08A 已完成，S-08B/S-08D 基础转换已实现，等待依赖环境测试
- 路线图任务：S-08
- 优先级：P2
- 前置依赖：S-04 Linux Surface、S-07 Window/Event 边界、H-08 公共 Canvas

## 背景与目标

Granit 核心不能强制依赖 SDL3 或 ImGui，但应提供经过维护的可选集成组件，减少应用重复编写原生
窗口提取、Surface 创建和 UI Draw Data 转换代码。

S-08 建立统一的 `integrations` 目录和目标命名规则，首批覆盖 SDL3 与 ImGui：

```text
include/granit/integrations/
├─ sdl3/
│  ├─ surface.hpp
│  └─ events.hpp          # 仅在确认需要无损转换后增加
└─ imgui/
   └─ renderer.hpp

src/integrations/
├─ sdl3/
│  └─ surface.cpp
└─ imgui/
   └─ renderer.cpp

tests/integrations/
├─ sdl3/
└─ imgui/

examples/integrations/
└─ sdl3_imgui/
```

目录表示与外部生态的组合边界，不是 Renderer Backend，也不是 Granit Window 的平台实现。

## 非目标

- 不让 `granit::granit`、`granit::window` 或 Renderer Vulkan 后端链接 SDL3、ImGui。
- 不把 `SDL_Window*`、`SDL_Event`、`ImDrawData` 等第三方类型放入 Granit 稳定 C ABI。
- 不在 Renderer 内为 SDL3、GLFW、Qt 等库分别建立重复 Surface Backend。
- 不由 Granit 接管 SDL3 事件循环、ImGui Context、字体资源或第三方对象生命周期。
- 首版不同时支持 SDL2；如有真实需求，单独建立版本明确的适配目标。

## 已确认决策

### CMake 目标与依赖

首版目标命名为：

```text
内部目标                         使用者目标
granit_integration_sdl3          granit::integration_sdl3
granit_integration_imgui         granit::integration_imgui
```

依赖保持单向：

```text
granit::integration_sdl3
├─ granit::granit
└─ SDL3::SDL3

granit::integration_imgui
├─ granit::granit
└─ ImGui 目标
```

- 两个目标独立启用、构建、安装和链接，可以任意组合。
- 第三方依赖只出现在对应 Integration 的接口中，不传播到未选择该 component 的使用者。
- 优先复用父项目已有目标，其次 `find_package`；第一版不默认下载或内置 SDL3、ImGui。
- 安装包通过独立 CMake component 暴露 Integration，基础 `granit::granit` 始终可单独使用。

### SDL3 集成职责

- 从调用方拥有的 `SDL_Window` 查询当前平台所需的 Win32、XCB 或 Wayland 原生值。
- 根据实际窗口后端选择 Granit Surface 类型并调用对应 Surface 创建入口。
- 不拥有或销毁 `SDL_Window`，不调用应用的主事件循环。
- 默认让应用直接处理 `SDL_Event`；只有无损语义与明确用例成立时，才增加可选 Event 转换。
- SDL3 版本和窗口属性接口差异封装在 Integration 内，不影响 Renderer 公共 ABI。

### ImGui 集成职责

- 消费调用方提供的 ImGui Draw Data，并转换成 Granit 公共 Canvas/Renderer 录制路径。
- 管理适配器自身需要的动态 Vertex/Index 数据、字体 Texture 和 Pipeline 缓存。
- ImGui Context、帧开始/结束、输入注入和平台窗口仍由应用或现有 ImGui Platform Backend 管理。
- 首版只实现 Renderer Backend 角色，不重复实现 SDL3/Win32 等 ImGui Platform Backend。
- 多 Viewport 支持作为后续子阶段，需要先明确额外平台窗口的所有权和 Surface 生命周期。

## 实施顺序

1. S-08A（已完成）：已建立 `integrations` 目录、可选构建开关、目标命名、依赖复用和安装
   component。
2. S-08B（基础实现完成，等待依赖环境测试）：已实现 SDL3 Window 到 Granit Surface 的适配；
   Win32 与 Wayland 直接借用原生值，X11 在 X11-xcb 可用时转换到 XCB connection。
3. S-08C：增加 SDL3 窗口、事件循环、Swapchain、Resize 和 Present 示例与集成测试。
4. S-08D（基础实现完成，等待依赖环境测试）：已实现 ImGui Draw Data 到 Granit Canvas 的转换，
   覆盖顶点/索引偏移、FramebufferScale、剪裁和调用方 Texture resolver。
5. S-08E：增加 SDL3 + ImGui 组合示例，验证输入、剪裁、纹理、Resize 和颜色空间。
6. S-08F：测量动态上传、Draw 合批和字体纹理更新；再决定多 Viewport 与事件转换范围。

## 测试与验收

- 禁用所有 Integration 时，核心构建、安装导出和 Consumer 结果不变。
- 父项目已提供 SDL3 或 ImGui 目标时不重复导入；缺少依赖时给出明确配置结果。
- SDL3 集成至少在 Win32、XCB、Wayland 可用环境分别完成 Surface、Swapchain 和 Present。
- ImGui 集成覆盖顶点与索引偏移、剪裁矩形、多纹理、字体 Atlas 和空 Draw Data。
- SDL3 + ImGui 示例不包含 Vulkan 头文件，不直接调用 Vulkan API。
- 共享库和静态库分别验证 Integration 的编译定义、符号导出和运行时依赖部署。
- 安装导出审计确认未选择 Integration 的 Consumer 不需要 SDL3 或 ImGui。

## 风险与未决问题

- SDL3 不同视频后端提供的原生属性及可用时间可能不同，需要按后端验证而非只按操作系统判断。
- ImGui 官方 API 不承诺稳定 C ABI，Integration 更适合作为同工具链构建的 C++ 可选组件。
- ImGui Texture ID 与 Granit Texture/View 句柄的映射必须校验生命周期，不能保存裸 Vulkan 描述符。
- 大量 UI Draw Call 可能需要专用批量上传与合批策略，但应先测量再扩展核心 Renderer。
- ImGui 多 Viewport 会动态创建额外平台窗口，需在 S-07 Window 和外部 Platform Backend 两种模式间
  明确唯一所有者。
