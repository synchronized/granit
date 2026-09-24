<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-56：Window Target 与 SDL3 后端

## 状态

**已完成。** 通用 Target、原生与 SDL3 Emscripten 多 Canvas、后端操作表、桌面 SDL3 Window
后端、Model Viewer 消费者迁移和当前行为文档已经完成。Pull Request #85 已通过 Windows
shared/static、Linux XCB/Wayland 与 SDL3、Emscripten、浏览器和安装 Consumer 验收。

## 背景与目标

当前 Window component 已统一 Win32、XCB、Wayland 与 Emscripten 的窗口、输入和 Surface 流程，
但 Emscripten Window 固定绑定 `#canvas`，并通过单个全局活动窗口限制规避 Canvas 身份冲突。
另一方面，SDL3 目前只提供外部 `SDL_Window` 到 Granit Surface 的适配，不能通过统一 Window API
管理窗口、事件和输入。

本任务建立两个正交概念：

- Window Backend 决定由原生平台实现还是 SDL3 管理窗口与事件；
- Window Target 描述一个 Window 绑定的外部目标，首个明确类型为浏览器 Canvas selector。

目标如下：

- 一个 Emscripten Window System 可按不同 selector 管理多个 Canvas；
- 原生 Emscripten 与 SDL3 Emscripten 后端消费同一份 Target 描述；
- 应用使用统一 `granit::window_system`、`granit::window`、事件、输入和 Surface API；
- SDL3 保持可选依赖，未启用时不进入 Window component 的公共依赖面；
- 后续可增加嵌入或采用外部窗口等 Target 类型，而不再增加平台专用字段。

## 非目标

- 不让 Granit 自动创建、插入或拥有浏览器 DOM Canvas；页面壳仍负责目标元素。
- 不把 SDL3 类型、事件或头文件暴露到 Granit Window 公共接口。
- 不改变现有外部 `SDL_Window` 创建 Surface 的 IntegrationSDL3 入口。
- 不在首阶段增加原生子窗口、编辑器嵌入句柄或移动平台 Window Target。
- 不承诺不同后端产生平台本身不存在的事件或完全相同的窗口管理能力。

## 已确认决策

公共模型为：

```text
Window System
  └─ Backend: automatic / win32 / xcb / wayland / emscripten / sdl3
       └─ Window
            └─ Target: automatic / canvas selector
```

- `granit_window_desc` 末尾追加可选的 `granit_window_target_desc*`；旧尺寸描述继续表示
  `automatic`。
- Target 是创建期借用描述。后端在 `granit_window_create` 返回前复制所需内容，不保存调用方指针。
- `automatic` 在桌面创建普通顶层窗口；原生 Emscripten 的首个自动目标兼容 `#canvas`。
- 同一进程中的活动 Window 不得绑定重复 Canvas selector，冲突返回
  `GRANIT_ERROR_RESOURCE_IN_USE`。
- 第二个 Emscripten 自动目标不生成隐式名称，若 `#canvas` 已占用则返回资源占用；多 Canvas 必须
  显式指定 selector。
- Canvas selector 为空、过长或包含内嵌空字符时返回 `GRANIT_ERROR_INVALID_ARGUMENT`；目标元素
  不可用时返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`。
- 后端不支持某个有效 Target 类型时返回 `GRANIT_ERROR_UNSUPPORTED`，不得静默忽略。
- SDL3 作为 Window 的可选编译后端实现，启用时由 `granit::window` 私有使用 SDL3；公共 Window
  头不包含 SDL3，未启用构建选择 SDL3 时明确返回不支持。
- SDL3 后端维护一个 Window System 级事件泵，并以 `SDL_WindowID` 映射 Granit Window handle；
  不为每个 Window 独立消费 SDL 全局事件队列。
- SDL3 Emscripten 创建窗口时把 Canvas selector 写入
  `SDL_PROP_WINDOW_CREATE_EMSCRIPTEN_CANVAS_ID_STRING`，创建后以 SDL 返回的实际属性为准。

长期架构依据见
[ADR-007：分离 Window Backend 与 Window Target](../decisions/ADR-007-window-backend-and-target.md)。

## 实施顺序

1. **S-56A Target 公共契约（已完成）**：增加 C11 描述、C++20 强类型包装、版本尺寸、参数校验
   和头文件契约测试；桌面后端明确拒绝 Canvas Target。
2. **S-56B 原生 Emscripten 多 Canvas（已完成）**：按 Window 保存 selector，几何、输入回调、
   原生查询和 Surface 创建均使用该值；已删除单活动窗口限制、检测重复目标，并通过 Chrome
   双 Canvas 创建、冲突和销毁后重建验证。
3. **S-56C Window 内部分发边界（已完成）**：Window System 创建时固定后端操作表，公共的系统、
   窗口和 Surface API 不再散落 SDL3 判断；平台专用原生句柄查询继续显式校验后端类型。
4. **S-56D 可选 SDL3 后端（已完成）**：桌面系统与多窗口生命周期、事件和输入转换、状态查询、
   Surface 创建及单活动 System 约束已经落地；IntegrationSDL3 的外部窗口入口保持不变。SDL3
   Emscripten 已通过 Chrome 双 Canvas 创建、重复 selector 拒绝、销毁和重新创建验收。
5. **S-56E 消费者迁移（已完成）**：Model Viewer Desktop/Web 共用
   `viewer_input_accumulator`，统一拖动、滚轮、快捷键、焦点、UI 捕获和背压语义。Desktop 已改用
   `granit::window` SDL3 后端、统一 Surface API 和 Granit Input → ImGuiIO 适配，同时保持独立
   渲染线程与 Frame Packet 语义不变；原 SDL Event 翻译层已删除。
6. **S-56F 文档与发布收口（本地完成，待远端）**：Window、Input、Integration、Compatibility、
   Architecture、构建和 Model Viewer 文档已同步；本地平台矩阵已通过，剩余 Linux XCB/Wayland
   与 SDL3 组合由远端矩阵验收。

本地实施和验证结果见
[S-56 Window Target 与 SDL3 后端本地验收](../records/2026-09-24-s56-window-sdl3-local-acceptance.md)。

## 测试与验收

- C11 与 C++20 公共头测试覆盖枚举值、结构尺寸、默认值和强类型 Target 构造。
- Window 契约测试覆盖旧版本描述、未知 Target、无效字符串、后端不支持和重复目标。
- Emscripten 浏览器测试创建两个不同 Canvas，分别验证尺寸、输入归属、Surface 和销毁后重建。
- SDL3 后端在桌面覆盖双窗口事件路由、关闭、Resize、输入状态和 Surface；在 Emscripten 覆盖
  两个 selector 不冲突。
- 未启用 SDL3 时 Window 的构建、安装和使用者依赖不包含 SDL3；启用时 shared/static Consumer
  均能解析依赖。
- Windows 动态与静态、Linux XCB/Wayland、Emscripten 构建及相关 CTest 通过。
- `git diff --check`、文档链接检查和安装 component 选择测试通过。

## 风险与未决问题

- 浏览器键盘事件只有在 Canvas 可聚焦并获得焦点时才能可靠归属；实现与测试需明确页面壳的
  `tabindex` 约束，不能继续把所有键盘事件绑定到全局 Window。
- SDL 事件队列属于进程级资源；多个 Granit SDL3 Window System 同时处理事件会产生所有权冲突。
  首版可限制进程内只有一个活动 SDL3 Window System，并返回资源占用。
- 下载模式启用 SDL3 后端会随 Window 安装锁定版本 SDL3 的运行库和包配置；系统依赖模式仍需由
  SDK 打包环境提供对应 SDL3 运行库。
- Model Viewer 桌面端仍拥有主线程 UI 和后台渲染线程；统一 Window API 不改变其 Frame Packet
  与渲染线程所有权边界。
- SDL 3.4.10 的 Emscripten 后端注销拖放事件时会重复删除 `/tmp/filedrop`；当前仅在窗口销毁调用
  期间兼容该上游行为，后续升级 SDL 时应复查并删除兼容分支。
