<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Input component

## 当前能力

`granit::input` 是依附于 `granit::window` 的可选组件，提供键盘、已提交文本和指针的事件与状态。
Win32、XCB 和 Wayland 后端已实现。SDL3、GLFW、Qt 和完整引擎应继续使用自身输入系统，不要求
经过 Granit Input。

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS Window Input)
target_link_libraries(app PRIVATE granit::window granit::input)
```

## 生命周期与线程

一个 Input System 附着到一个 Window System：

```cpp
granit::window_system windows;
windows.initialize();

granit::input_system input;
input.initialize(windows.native_handle());
```

- Input System 必须在 Window System 的创建线程创建、轮询和销毁。
- 一个 Window System 只能附着一个 Input System。
- 必须先销毁 Input System，再销毁 Window System。
- Window 销毁后会移除对应状态和待处理输入事件。
- `poll` 会非阻塞泵送平台消息，但不会消费 Window 事件队列。

## 事件与状态

`granit_input_poll_event` 返回一次性输入变化；队列为空时返回 `GRANIT_ERROR_NOT_READY`。事件包括：

- 物理键、逻辑键、修饰键和按下、重复、抬起动作。
- 固定容量 UTF-8 已提交文本。
- 指针进入、离开、移动、按钮和水平/垂直滚轮。

`granit_input_get_keyboard_state` 查询 0～255 USB HID usage 位图与当前修饰键；
`granit_input_get_pointer_state` 查询相对窗口客户区的逻辑坐标、按钮位图和指针是否在窗口内。
指针坐标不是 framebuffer 像素，渲染代码应结合 Window Scale 事件换算。

焦点丢失时，所有按键和指针按钮状态会被清除，避免产生卡键。状态清理不伪造逐键抬起事件。

## Win32 语义

首版以窗口消息作为唯一权威输入源，不同时消费 Raw Input，避免重复事件：

- `WM_KEYDOWN/UP` 与 `WM_SYSKEYDOWN/UP` 提供扫描码、逻辑键和重复位。
- `WM_CHAR` 与 `WM_UNICHAR` 转换为 UTF-8；UTF-16 代理对会合并后提交。
- Mouse Move、Button、Wheel 和 Leave 消息更新指针事件与状态。
- 滚轮消息的屏幕坐标在 Window 层转换为客户区坐标。

平台文本会先经过严格 UTF-8 校验，再按完整码点边界拆入 48 字节事件负载；不依赖第三方 Unicode
库。Input 不执行规范化、字素切分或文字整形，这些能力属于上层文本系统。

Win32 扫描码、UTF-16 和 Mouse 消息解码位于 Input DLL 的私有平台 adapter；通用运行时只接收
归一化事件并维护队列和状态。该 adapter 不是公共 API，也不会把 Win32 类型暴露到安装头文件。

物理键优先映射常用 USB HID Keyboard/Keypad usage。当前枚举未覆盖的扫描码返回
`GRANIT_PHYSICAL_KEY_UNKNOWN`；可打印字符只通过文本事件提交，不塞入逻辑键枚举。

## XCB 语义

XCB Window 订阅核心键盘和指针事件，并在同一事件泵中转交 Input：

- Xorg 常用 evdev keycode 映射为 USB HID 物理键；导航键和功能键同时提供逻辑键。
- 指针支持进入、离开、移动、主键、次键、中键、扩展键及水平、垂直滚轮。
- XCB FocusOut 清理键盘与指针按钮状态。

当前实现不引入 `xkbcommon`。因此 XCB 暂不提供布局相关文本提交，非标准 keycode 映射为
`GRANIT_PHYSICAL_KEY_UNKNOWN`；布局、文本和自动重复细节将在 Wayland 键盘依赖评审时统一处理。

## Wayland 语义

Wayland Window 在既有事件泵中管理 `wl_seat`，Input 不单独读取 Display 队列：

- `wl_keyboard` keymap 由 Input 私有链接的 `libxkbcommon` 解析，生成物理键、逻辑键和 UTF-8
  已提交文本。
- `wl_pointer` 提供相对 Surface 的逻辑坐标、移动、按钮、进入、离开和滚轮事件。
- keyboard leave 会清理键盘与指针按钮状态，避免焦点切换后卡键。
- 缺少 `libxkbcommon` 时仍可构建 Wayland Window，但 Wayland Input 创建返回不支持。

`libxkbcommon` 不出现在公共头文件中；共享库构建将其保持为 Input 的私有运行时依赖。静态构建
需要最终应用链接系统 `libxkbcommon`。首版不实现客户端重复计时器和 Compose/IME 预编辑；这些
能力需要与应用事件循环和文本服务共同设计。

## 当前限制

- 不支持手柄、触摸、手写笔、相对鼠标、捕获、指针约束、剪贴板和拖放。
- 不提供 IME 预编辑、候选窗或组合文本协议，只提供已经提交的文本。
- 不提供 Action Mapping、快捷键系统或外部事件注入。
- XCB 布局文本、Wayland 客户端按键重复和 Compose/IME 仍需单独实现、验证。
