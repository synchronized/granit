<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Window 输入

## 当前能力

Window component 内建键盘、已提交文本和指针的事件与状态，支持 Win32、XCB 和 Wayland。应用只
创建一个 Window System，不再创建独立 Input System，也不需要链接第二个动态库。

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS Window)
target_link_libraries(app PRIVATE granit::window)
```

SDL3、GLFW、Qt 和完整引擎继续使用自身输入系统；Granit 不把外部窗口库的输入转换为 Window
Input。

## 帧循环

应用每轮先显式处理一次平台事件，再分别读取 Window 与 Input 队列：

```cpp
granit::window_system windows;
windows.initialize();

while (running) {
  windows.process_events();

  granit::window_event window_event = GRANIT_WINDOW_EVENT_INIT;
  while (windows.poll(window_event) == granit::result::success) {
    // 处理关闭、尺寸、焦点与缩放。
  }

  granit::input_event input_event = GRANIT_INPUT_EVENT_INIT;
  while (windows.poll(input_event) == granit::result::success) {
    // 处理键盘、文本与指针变化。
  }
}
```

`granit_window_system_process_events` 是唯一平台事件泵。
`granit_window_poll_event` 和 `granit_window_poll_input_event` 只弹出已有事件；队列为空返回
`GRANIT_ERROR_NOT_READY`，不会隐式读取平台队列。两个队列分别保持产生顺序，但不提供跨类型的
公共总顺序。

所有操作必须在 Window System 创建线程执行。Window 销毁会同步移除对应状态和待处理输入事件；
Window System 销毁会直接回收全部输入状态，没有额外的 Input 销毁顺序。

## 事件与状态

`granit_window_poll_input_event` 返回一次性变化：

- 物理键、逻辑键、修饰键和按下、重复、抬起动作。
- 固定容量 UTF-8 已提交文本。
- 指针进入、离开、移动、按钮和水平、垂直滚轮。

`granit_window_get_keyboard_state` 查询 0～255 USB HID usage 位图与当前修饰键；
`granit_window_get_pointer_state` 查询相对窗口客户区的逻辑坐标、按钮位图和指针是否在窗口内。
指针坐标不是 framebuffer 像素，渲染代码应结合 Window Scale 事件换算。

焦点丢失时会清除全部按键和指针按钮状态，避免产生卡键。状态清理不伪造逐键抬起事件。

## Win32 语义

Win32 使用窗口消息作为唯一权威输入源，不同时消费 Raw Input：

- `WM_KEYDOWN/UP` 与 `WM_SYSKEYDOWN/UP` 提供扫描码、逻辑键和重复位。
- `WM_CHAR` 与 `WM_UNICHAR` 转换为 UTF-8；UTF-16 代理对会合并后提交。
- Mouse Move、Button、Wheel 和 Leave 消息更新指针事件与状态。
- 滚轮消息的屏幕坐标在 Window 层转换为客户区坐标。

平台文本先经过严格 UTF-8 校验，再按完整码点边界拆入 48 字节事件负载。Window 不执行规范化、
字素切分或文字整形，这些能力属于上层文本系统。

## XCB 与 Wayland 语义

XCB Window 在同一个事件泵中处理核心键盘和指针事件。常用 evdev keycode 映射为 USB HID 物理键，
导航键和功能键同时提供逻辑键；指针支持进入、离开、移动、按钮和滚轮。XCB 当前不依赖
`xkbcommon`，因此不提供布局相关文本，未知 keycode 映射为
`GRANIT_PHYSICAL_KEY_UNKNOWN`。

Wayland Window 管理 `wl_seat`、`wl_keyboard` 与 `wl_pointer`。构建时存在 `libxkbcommon` 才启用
键盘 keymap、逻辑键和 UTF-8 文本转换；缺少依赖时 Window 仍可构建并处理窗口事件。共享库将
`libxkbcommon` 保持为私有依赖，静态应用由导入目标补充最终链接依赖。

## 当前限制

- 不支持手柄、触摸、手写笔、相对鼠标、捕获、指针约束、剪贴板和拖放。
- 不提供 IME 预编辑、候选窗或组合文本协议，只提供已经提交的文本。
- 不提供 Action Mapping、快捷键系统或外部事件注入。
- XCB 布局文本、Wayland 客户端按键重复和 Compose/IME 仍需单独实现、验证。
