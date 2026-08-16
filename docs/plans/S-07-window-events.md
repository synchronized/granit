<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-07：Window、Event 与 Input 边界

## 状态

- 设计状态：已确认
- 实现状态：S-07A 契约完成，S-07B 待开始
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

### 组件与导出边界

- Window 构建为独立可选动态库 `granit_window`，使用者目标为 `granit::window`。
- Window 使用独立 `GRANIT_WINDOW_API` 与 `GRANIT_WINDOW_STATIC_DEFINE`，不复用核心 DLL 的
  `GRANIT_API` 导入导出状态。
- Window 只共享 `granit_handle`、`granit_result` 等基础 ABI 头，不链接 Renderer，也不包含 Vulkan。
- 安装包使用独立 `Window` component；未请求该 component 的 Consumer 不加载窗口系统依赖。

### 句柄与所有权

第一版包含两个 64 位句柄：

- `granit_window_system`：平台连接、系统事件抽取和事件队列的所有者。
- `granit_window`：由指定 Window System 创建并拥有的单个顶层窗口。

零值无效。Window 句柄校验 generation、类型和所属 Window System，不能跨 System 操作。销毁
Window System 前应先销毁全部 Window；若仍有残留，验证模式报告遗漏后执行级联清理。

创建描述使用以下稳定字段：

```c
typedef struct granit_window_desc {
  uint32_t struct_size;
  const char* title;
  uint32_t title_length;
  uint32_t width;
  uint32_t height;
  uint32_t flags;
  uint32_t reserved;
} granit_window_desc;
```

标题是调用期间借用的 UTF-8 字节序列；宽高使用客户区逻辑尺寸。首版标志覆盖初始可见、可调整尺寸
和高 DPI，未知标志返回 `GRANIT_ERROR_INVALID_ARGUMENT`。

### 原生窗口值

Window 不返回 Renderer 的 Surface 描述，避免建立 `Window -> Renderer` 依赖。它提供三个明确的
平台查询入口：

```c
granit_window_get_win32(window_system, window, &instance, &hwnd);
granit_window_get_xcb(window_system, window, &connection, &xcb_window);
granit_window_get_wayland(window_system, window, &display, &wl_surface);
```

查询只借出原生值，不转移所有权；平台不匹配时返回 `GRANIT_ERROR_UNSUPPORTED`。调用方把查询结果
传给现有 `granit_surface_create_*`。原生值只保证在 Window 存活且未发生平台对象重建时有效；
Wayland 隐藏/重显等变化通过专用原生对象变化事件通知调用方重建 Surface。

### Event 模型

第一版采用调用方主动轮询的值类型事件队列，不使用默认回调：

```c
granit_window_event event;
while (granit_window_poll_event(window_system, &event) == GRANIT_SUCCESS) {
  /* 根据 event.type 处理关闭、尺寸、焦点或输入。 */
}
```

事件固定头包含 `struct_size`、类型、所属 Window 和纳秒单调时间戳。S-07 第一版只定义：

- `close_requested`：应用决定是否真正销毁，不在回调中隐式销毁。
- `resized`：携带 framebuffer 像素宽高，用于 Swapchain 重建。
- `focus_changed`：携带获得或失去焦点状态。
- `scale_changed`：携带内容缩放比例和新的 framebuffer 像素尺寸。
- `native_handle_changed`：原生窗口对象变化，旧 Surface 不得继续使用。

负载采用固定大小、显式字段的联合体；结构预留空间并以 `struct_size` 协商，不保存平台指针。键盘、
指针、触摸、手柄与文本输入不进入首版 Window Event，待 S-07E 依据真实需求设计 Input component。

`granit_window_poll_event` 每次弹出一个事件；队列为空返回 `GRANIT_ERROR_NOT_READY`，不是错误日志。
第一版不提供阻塞等待，应用可以结合自身主循环控制节奏。队列属于 Window System，统一承载其所有
窗口事件；同一时刻只能由创建 Window System 的线程抽取。

轮询模型的主要收益是 C ABI 生命周期明确、无回调重入、容易接入主循环，并便于测试、录制和回放。
代价是应用必须及时抽取队列，且连续 Resize、焦点和缩放事件需要明确的合并策略与容量上限。

### 线程与错误语义

- Window System、Window 创建销毁、属性修改和事件轮询均限制在创建 System 的线程。
- 线程错误返回 `GRANIT_ERROR_INVALID_ARGUMENT`，不尝试跨线程转发平台消息。
- 队列为空返回 `GRANIT_ERROR_NOT_READY`；平台连接断开返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`。
- 不支持的 Window 后端或原生值查询返回 `GRANIT_ERROR_UNSUPPORTED`。
- 输出参数在失败时清零，异常不得穿过 C ABI。
- Resize、焦点和缩放等可覆盖状态事件允许在入队前合并；关闭和原生对象变化事件不得丢弃。

### 第三方接入

- Granit Window 是便利组件，不是 Renderer 的必选前置层。
- SDL、GLFW、Qt 或引擎平台层继续负责自身窗口和事件，并向 Surface 提供原生平台值。
- 不把第三方窗口类型放入稳定 C ABI；适配代码使用独立可选目标或由使用者维护。
- Window 销毁前必须先停止对应帧循环，并按 Swapchain、Surface、原生窗口的顺序释放。

## 实施顺序

1. S-07A（已完成）：已确认独立组件导出边界、C11 句柄、窗口描述、平台专用原生值查询、
   Window Event 范围、队列与线程错误语义。
2. S-07B：实现 Win32 Window 与轮询事件队列，迁移一个现有窗口示例验证边界。
3. S-07C：实现 XCB Window，并与 S-04 的 Surface、Swapchain 和 Present 测试组合。
4. S-07D：实现 Wayland Window；窗口角色和 configure 流程由 Window 后端管理。
5. S-07E：评估是否独立导出 Input component；SDL3 与其他第三方接入由
   [S-08 Integration 计划](S-08-third-party-integrations.md)承接。

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
- 固定大小事件负载的具体字节数和保留字段布局在公共头落地时由 C11/C++20 ABI 测试锁定。
- 队列容量和可覆盖状态事件的合并阈值需由 Win32 原型与多窗口压力测试确定。
- Input 是否成为独立安装 component，等待键鼠之外的真实需求再决定。
