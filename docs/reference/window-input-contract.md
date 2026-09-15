<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Window 与 Input 契约

本文汇总 Window 生命周期、输入状态、错误、结构扩展和线程规则。平台事件映射见
[Window](window.md)与[Window 输入](input.md)。

## Component 与依赖

- `Window` component 提供唯一目标 `granit::window`。
- Window System 同时拥有窗口、Window Event、Input Event、键盘状态和指针状态。
- 输入值类型位于 `<granit/window/input.h>` 与 `<granit/window/input.hpp>`。
- 平台适配与 xkbcommon 保持为私有实现，不进入公共头文件。
- Core Renderer 和 RenderPipeline 不依赖 Window；外部窗口所有者可以绕过 Window component。

## 所有权与销毁顺序

- Window System 拥有其创建的 Window；单独销毁 Window 会使句柄立即失效，销毁 Window System
  会级联销毁剩余 Window。
- Window 销毁同步移除对应键盘状态、指针状态和待处理输入事件。
- 窗口状态查询只复制尺寸与缩放值；原生窗口、Display、Connection 和 Surface 查询只借出值，
  不转移所有权。
- 不存在独立 Input System、Input 句柄或额外销毁顺序。

## 事件处理

`granit_window_system_process_events` 非阻塞地处理当前线程已经到达的平台消息。一次处理可以同时
更新窗口状态、输入状态和两个事件队列。

`granit_window_poll_event` 与 `granit_window_poll_input_event` 只读取各自队列，不隐式调用事件泵。
队列分别保持事件产生顺序；队列为空返回 `GRANIT_ERROR_NOT_READY`。应用通常每帧处理一次事件，
然后分别清空两个队列。

## 错误与输出

- 空指针、非法描述和跨线程调用返回 `GRANIT_ERROR_INVALID_ARGUMENT`；失效、类型错误或归属错误
  的句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。
- 请求与当前平台不匹配的原生值返回 `GRANIT_ERROR_UNSUPPORTED`，输出值清零。
- 平台连接不可用返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`。
- 创建失败时输出句柄为零。事件、窗口状态和输入状态查询在失败时返回已清零的已知字段，调用者
  只应在成功时使用业务数据。

## `struct_size` 与输出容量

- 所有可扩展结构均提供固定的 `*_VERSION_1_SIZE`；输入描述小于最低尺寸时返回参数错误，未知
  尾部被忽略。
- 调用者使用 `*_INIT` 初始化事件和状态输出。函数读取调用者提供的 `struct_size` 作为容量，只写
  容量与当前结构大小中的较小值，不覆盖调用者的未知尾部。
- 返回结构中的 `struct_size` 表示本次实际写入字节数。Keyboard 和 Pointer 的 V1 尺寸不包含
  保留尾部，旧调用方只分配 V1 大小时也不会发生越界写入。

## 线程规则

Window System 绑定其创建线程。窗口创建和销毁、事件处理与轮询、状态和原生值查询都必须在该
线程执行；跨线程调用返回 `GRANIT_ERROR_INVALID_ARGUMENT`。平台消息处理不会让异常穿过动态库
或原生事件边界。
