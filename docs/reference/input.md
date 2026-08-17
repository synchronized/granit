<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Input 值类型

## 当前状态

S-07E1 已锁定 Input component 第一版的 C11 值类型 ABI。当前只提供事件与状态的数据结构；Input
System 的创建、轮询、平台事件桥和安装 component 将在 S-07E2 及后续阶段实现。

公共入口为 `<granit/input.h>` 和 `<granit/input.hpp>`。头文件不包含平台或 Vulkan 类型。

## 事件模型

`granit_input_event` 包含结构大小、事件类型、所属 Window、单调纳秒时间戳和 64 字节固定负载。
事件类型覆盖：

- 物理键、逻辑键、修饰键及按下、抬起、重复动作。
- 固定容量 UTF-8 已提交文本；不包含预编辑文本和候选区。
- 指针移动、按钮、滚轮、进入和离开。

文本负载容量为 `GRANIT_INPUT_TEXT_CAPACITY`（48 字节），`length` 指明有效字节数。平台提交超过
容量时应在 UTF-8 码点边界拆分为多个事件。

物理键枚举采用 USB HID Keyboard/Keypad usage 值，表达与布局无关的键盘位置。逻辑键只表达
Enter、方向键、功能键等非打印语义；可打印内容必须读取文本事件。

## 状态模型

`granit_keyboard_state` 使用四个 64 位整数记录 usage 0～255 的当前按下状态，并单独记录修饰键。
C++ 可使用 `granit::key_is_pressed` 查询物理键位。

`granit_pointer_state` 保存相对 Window 客户区的逻辑坐标、按钮位图和指针是否位于客户区。坐标不是
framebuffer 像素，应用应结合 Window Scale 事件换算渲染尺寸。

所有结构均带 `struct_size` 和初始化宏；保留字段必须保持为零。运行时 API 落地后，失败路径会清零
输出数据并恢复正确的 `struct_size`。
