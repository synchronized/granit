<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-07：Window、Event 与 Input 边界

## 状态

- 设计状态：已确认
- 实现状态：待开始
- 路线图任务：S-07
- 优先级：P2
- 前置依赖：S-04 Linux Surface

## 背景与目标

Renderer 当前接收调用方提供的 Win32、XCB 或 Wayland 原生值，并据此创建 Surface。该边界适合
SDL、GLFW、Qt 和完整引擎，但基础示例仍需重复实现平台窗口与事件循环。

S-07 计划增加可选 Window 组件，为简单应用和示例提供统一窗口入口，同时维持以下依赖方向：

```text
应用或引擎
├─ Window：窗口、显示器和窗口事件
├─ Input：输入状态与输入事件（后续按需求独立）
└─ Renderer：Surface、Swapchain 和 GPU 渲染
       ▲
       └─ 只借用 Window 或第三方库提供的原生窗口值
```

## 非目标

- 不让 Renderer 创建窗口、轮询操作系统事件或拥有原生窗口。
- 不强制应用使用 Granit Window；外部窗口库始终可以直接接入 Renderer Surface。
- 不在第一版建立全局 Event Bus、ECS 消息系统或跨线程任务调度器。
- 不把文件系统、网络、进程和线程等无关能力聚合进庞大的公共 `OS` 模块。
- 不在第一版完整覆盖手柄、触摸、输入法、剪贴板、拖放和窗口装饰。

## 已确认决策

### 模块职责

- `granit::renderer` 保留平台 Surface 后端，只负责原生窗口与 Vulkan 输出连接。
- 可选 `granit::window` 负责窗口创建、销毁、标题、尺寸、可见性、DPI、显示器与事件轮询。
- Window 提供与 Renderer 公共平台描述兼容的原生值，但不直接依赖或创建 Renderer。
- 内部平台差异放入 `src/platform/win32`、`src/platform/xcb` 和 `src/platform/wayland` 等目录；
  `platform` 是实现组织方式，不形成面向普通用户的大型 `granit::os` API。

### Event 模型

第一版采用调用方主动轮询的值类型事件队列，不使用默认回调：

```c
granit_window_event event;
while (granit_window_poll_event(window_system, &event) == GRANIT_SUCCESS) {
  /* 根据 event.type 处理关闭、尺寸、焦点或输入。 */
}
```

事件至少携带类型、所属窗口、单调时钟时间戳和对应负载。窗口关闭请求、尺寸、焦点和 DPI 属于
Window Event；键盘、指针、触摸、手柄和文本输入在规模扩大后可迁移到独立 Input 组件。

轮询模型的主要收益是 C ABI 生命周期明确、无回调重入、容易接入主循环，并便于测试、录制和回放。
代价是应用必须及时抽取队列，且高频指针事件需要合并策略和明确的队列容量上限。

### 第三方接入

- Granit Window 是便利组件，不是 Renderer 的必选前置层。
- SDL、GLFW、Qt 或引擎平台层继续负责自身窗口和事件，并向 Surface 提供原生平台值。
- 不把第三方窗口类型放入稳定 C ABI；适配代码使用独立可选目标或由使用者维护。
- Window 销毁前必须先停止对应帧循环，并按 Swapchain、Surface、原生窗口的顺序释放。

## 实施顺序

1. S-07A：确认 C11 句柄、窗口描述、原生值查询、事件布局和错误语义。
2. S-07B：实现 Win32 Window 与轮询事件队列，迁移一个现有窗口示例验证边界。
3. S-07C：实现 XCB Window，并与 S-04 的 Surface、Swapchain 和 Present 测试组合。
4. S-07D：实现 Wayland Window；窗口角色和 configure 流程由 Window 后端管理。
5. S-07E：评估是否独立导出 Input component，并补充 SDL、GLFW 和 Qt 接入指南。

## 测试与验收

- Window 公共 `.h` 可由 C11 编译器独立包含，不暴露平台或 Vulkan 头文件。
- C 与 C++ API 覆盖创建、轮询、关闭、Resize、DPI、重复销毁和无效句柄。
- 平台测试验证事件顺序、队列容量、高频事件合并和原生窗口生命周期。
- 同一 Renderer 能分别使用 Granit Window 和外部创建的窗口完成 Surface 与 Present。
- 禁用 Window component 后，Renderer、无窗口渲染和第三方窗口接入不受影响。
- 安装导出按 CMake component 隔离，平台依赖不传播到 Renderer 使用者。

## 风险与未决问题

- Wayland 的异步 configure、窗口角色和缩放语义不能机械套用 Win32/XCB 模型。
- 文本输入与按键不是同一概念；IME 与组合文本进入范围前需要单独设计。
- 多窗口事件应采用全局 Window System 队列还是每窗口队列，需由原型和主循环用例验证。
- 事件 ABI 的联合体布局、未知事件跳过方式和队列溢出行为需要在实现前固定。
- Input 是否成为独立安装 component，等待键鼠之外的真实需求再决定。
