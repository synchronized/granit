<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-07E：Input component 边界

## 状态

- 设计状态：已确认
- 实现状态：S-07E1～S-07E4 及安装 Consumer 已完成，等待 Wayland 输入适配
- 路线图任务：S-07E
- 优先级：P2
- 前置依赖：S-07 Window component

## 背景与目标

Window 已统一 Win32、XCB 和 Wayland 的窗口生命周期及平台事件泵，但键盘、指针和文本输入不应
进入 Window Event。S-07E 增加独立可选 Input component，并维持以下边界：

```text
应用或引擎
├─ Window：窗口生命周期、尺寸、焦点和关闭
├─ Input：键盘、指针、文本事件和当前状态
└─ Renderer：Surface、GPU 资源和绘制

Window 平台事件泵
├─ 窗口事件 -> Window 队列
└─ 原始输入 -> Input 队列与状态快照
```

Input 服务使用 Granit 原生 Window 的轻量应用和示例。SDL3、GLFW、Qt 或完整引擎仍可使用自身
输入系统，不要求经过 Granit Input。

## 非目标

- 第一版不支持手柄、触摸、手写笔、传感器、剪贴板和拖放。
- 第一版不提供完整 IME 候选窗、预编辑区和组合文本协议。
- 不建立全局 Event Bus、Action Mapping、快捷键系统或游戏玩法输入映射。
- 不把 Renderer、Scene 或 UI 依赖引入 Input。
- 不让 Input 单独读取与 Window 竞争的 Win32、XCB 或 Wayland 平台队列。

## 已确认决策

### 组件与依赖方向

- 公共 CMake 目标为 `granit::input`，安装 component 为 `Input`。
- Input 依赖 `granit::window` 的基础事件泵，不依赖 Renderer 或 Vulkan。
- 公共 C ABI 使用 `granit_input_system` 64 位句柄；零值无效。
- Input System 附着到一个 `granit_window_system`，不能跨 Window System 使用窗口句柄。
- Window 仍是唯一的平台事件读取者；内部桥接把输入分发给 Input，不向普通用户暴露平台结构体。
- 平台消息解码分别位于 `src/platform/win32`、`src/platform/xcb` 和
  `src/platform/wayland`；`src/input/input_api.cpp` 只管理句柄、事件队列、状态和生命周期。

内部桥接可以使用 Window 与 Input 两个组件之间的私有 sink，但必须满足：

- 只在 Window System 创建线程调用。
- 不把回调作为公共应用编程模型。
- Input 销毁时先注销 sink，不能留下悬空用户数据。
- `granit_input_poll_event` 可以请求 Window 执行一次非阻塞泵送，但不能消费 Window 队列。

### 事件与状态双模型

Input 同时提供两类入口：

- 事件：表达按下、抬起、重复、滚轮和文本提交等一次性变化。
- 状态：表达当前按键、指针位置和按钮状态，适合逐帧查询。

概念接口如下，名称和字段在实现前通过 C11 ABI 测试锁定：

```c
granit_input_poll_event(input_system, &event);
granit_input_get_keyboard_state(input_system, window, &keyboard);
granit_input_get_pointer_state(input_system, window, &pointer);
```

事件队列为空返回 `GRANIT_ERROR_NOT_READY`。事件和值结构使用 `struct_size`，失败时清零输出参数。

### 键盘语义

键盘输入分为三层，不能互相替代：

- 物理键：使用稳定扫描码表达键盘位置，优先映射 USB HID Keyboard/Keypad usage。
- 逻辑键：表达 Escape、方向键、功能键等布局后的语义；可打印字符不塞入逻辑键枚举。
- 文本输入：使用 UTF-8 表达已经提交的文本，供输入框和控制台使用。

第一版覆盖物理键、常用逻辑键、修饰键和已提交文本。文本事件使用固定容量 UTF-8 分片及显式
长度；一个平台文本提交可以产生多个事件。预编辑文本和候选区留给后续 IME 扩展。

按键事件至少携带物理键、逻辑键、修饰键和按下/抬起/重复状态。焦点丢失时 Input 必须合成状态
清理，避免应用观察到永久按下的“卡键”。

### 指针语义

第一版包含：

- 相对窗口客户区的逻辑坐标。
- 相对移动量。
- 主键、次键、中键和扩展按钮位图。
- 水平与垂直滚轮增量。
- 进入、离开和移动事件。

指针坐标不使用 framebuffer 像素；应用结合 Window Scale 事件转换到渲染尺寸。第一版不提供全局
桌面坐标、鼠标捕获、相对模式和指针约束，这些能力需要逐平台验证后单独扩展。

### 生命周期与线程

- Input System 必须在所附着 Window System 的创建线程创建、轮询和销毁。
- Window System 销毁前必须先销毁全部 Input System；否则验证模式报告并安全注销。
- Window 销毁时丢弃其待处理输入事件，并使针对该窗口的状态查询返回无效句柄。
- 一个 Window System 可以附着一个 Input System；第一版不支持多个消费者复制同一输入流。
- Input 事件只借用或内嵌数据，不跨动态库边界转移内存所有权。

### 第三方输入系统

- SDL3、GLFW 和引擎自己的输入循环不转换为 Granit 原生 Window 事件。
- S-08 可以提供独立 SDL3 Input adapter，但不能让 SDL3 成为 `granit::input` 的传递依赖。
- 若后续需要外部事件注入，应设计明确标记的不稳定 adapter/feed 接口，不开放伪造系统输入的
  基础 API。

## 实施顺序

1. S-07E1（已完成）：已锁定 C11 句柄、事件头、物理键、逻辑键、修饰键、固定容量 UTF-8
   文本负载和键盘/指针状态结构布局，并增加 C11/C++20 公共头编译测试。
2. S-07E2（已完成）：已增加独立 Input 目标、安装 component、C++20 RAII 包装和 Window
   内部分发桥；轮询会非阻塞泵送平台消息但不消费 Window 事件队列。
3. S-07E3（已完成）：以 Win32 窗口消息作为唯一权威输入源，覆盖物理/逻辑键、重复、UTF-8
   文本提交、指针进入/离开/移动、按钮、滚轮和焦点丢失状态清理；不同时消费 Raw Input，避免
   重复事件。Win32 解码位于内部平台 adapter，通用运行时只管理归一化事件、状态和生命周期。
4. S-07E4（已完成）：XCB 核心事件已覆盖常用物理键、导航逻辑键、重复状态、指针边界、
   移动、按钮、滚轮和焦点丢失清理。当前不引入 XKB，布局文本与更完整重复语义留待 S-07E5
   统一评审。
5. S-07E5：实现 Wayland `wl_seat`、keyboard 和 pointer；键盘映射依赖在引入前单独确认。
6. S-07E6（部分完成）：安装后的 C11/C++20 Consumer 已覆盖 Input component 发现、动态/静态
   链接类型一致性、创建销毁和无图形会话降级；跨平台事件回归随 S-07E5 补齐。

## 测试与验收

- Input 公共 `.h` 可由 C11 编译器独立包含，不暴露 Win32、XCB、Wayland 或 Vulkan 类型。
- 动态库和静态库均覆盖创建、销毁、无效句柄、跨线程和错误输出清零。
- 键盘测试区分物理键、逻辑键、文本提交和重复按键。
- 指针测试覆盖坐标、相对移动、按钮、滚轮、进入与离开。
- 焦点丢失和窗口销毁不会留下按下状态或悬空事件。
- Input 轮询不会吞掉 Window Resize、Close 或 Scale 事件，反向亦然。
- 禁用 Input component 后，Window、Renderer 和第三方输入路径不受影响。
- 安装后 C 与 C++ Consumer 不依赖开发机源码目录或未声明的第三方包。

## 风险与未决问题

- XCB 基础键鼠不依赖 xkbcommon；布局文本是否与 Wayland 共用 xkbcommon，留待 S-07E5 决定。
- Wayland 键盘映射通常需要 xkbcommon；版本、许可证和静态链接传播需要单独确认。
- Win32 传统消息与 Raw Input 的组合可能产生重复事件，必须选定唯一权威来源。
- UTF-8 固定分片容量、事件队列容量和高频指针移动合并策略需要原型测量。
- 相对鼠标、捕获、手柄、触摸和完整 IME 只在出现真实使用场景后进入后续计划。
