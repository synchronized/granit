<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Input component

## 当前能力

`granit::input` 是依附于 `granit::window` 的可选组件，提供键盘、已提交文本和指针的事件与状态。
当前 Win32 后端已实现；XCB 与 Wayland 输入仍在计划中。SDL3、GLFW、Qt 和完整引擎应继续使用
自身输入系统，不要求经过 Granit Input。

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

物理键优先映射常用 USB HID Keyboard/Keypad usage。当前枚举未覆盖的扫描码返回
`GRANIT_PHYSICAL_KEY_UNKNOWN`；可打印字符只通过文本事件提交，不塞入逻辑键枚举。

## 当前限制

- 不支持手柄、触摸、手写笔、相对鼠标、捕获、指针约束、剪贴板和拖放。
- 不提供 IME 预编辑、候选窗或组合文本协议，只提供已经提交的文本。
- 不提供 Action Mapping、快捷键系统或外部事件注入。
- XCB 键盘布局与 Wayland Seat 的依赖和映射仍需单独实现、验证。
