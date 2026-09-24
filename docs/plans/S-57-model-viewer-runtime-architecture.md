<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-57：Model Viewer 运行时架构

## 状态

**实施中。** S-55 已统一资产与 glTF 加载边界，S-56 已统一 Desktop/Web Window 和输入语义。
Viewer Core 纯渲染帧与执行层 Frame Packet 已完成分离；Desktop 渲染命令已收敛到专用服务，
下一步继续转移剩余 GPU 资源所有权，不改变现有目录入口、用户功能或线程模型。

## 背景

当前共享层已经具备明确边界：

- `examples/common/model_viewer` 的 `granit_example_model_scene` 同时服务 PBR 教程和完整 Sample，
  负责 glTF Scene 到 GPU Scene 的规划、上传和绑定，不属于平台壳；
- `application_core` 保存 Viewer 状态、CPU/GPU Scene、环境和相机，生成后端无关帧数据；
- `frame_executor` 明确区分 Web inline 执行与 Desktop 有界线程执行；
- `viewer_input_accumulator` 直接消费统一 Window/Input 事件，保留 Viewer 专用键位和背压语义。

主要问题已经从“后端重复”转为“平台入口承担过多职责”：

| 位置 | 当前职责 |
|---|---|
| `desktop/main.cpp` | 参数、Window、ImGui、资产加载、CPU 规划、GPU 命令、线程队列、呈现恢复、性能输出 |
| `web/application.cpp` | Host、Renderer 启动、资产加载、Pipeline 预热、帧执行、控制接口、浏览器验收导出 |
| `application_core.h` | Viewer 领域帧数据，同时直接携带 Desktop 才使用的 ImGui Canvas 数据 |

文件长度本身不是问题；问题是所有权、调用线程、可丢弃帧与不可丢弃控制命令需要跨越大段入口代码
才能确认，导致修改呈现恢复、加载或 UI 时容易触及无关生命周期。

## 目标

- 让 Desktop `main.cpp` 只负责参数解析、顶层对象装配、运行和退出结果；
- 让 Web `application.cpp` 不再同时实现产品运行时、Pipeline 验收算法和全部 C 导出；
- 分离 Viewer Core 生成的渲染数据与执行器消费的 UI/调度信封；
- 用类型和对象所有权表达 Desktop 主线程、渲染线程及 Web 浏览器主线程边界；
- 保持 Desktop 背压、可替换帧、不可丢弃控制命令和性能采样行为；
- 保持 Web inline executor、Asyncify 进度让出、浏览器 C ABI 验收和 JavaScript 导出名称；
- 所有新增实现继续位于现有 `examples/samples/model_viewer` 目录树，不调整用户已确认的路径。

## 非目标

- 不把 Model Viewer、执行器、加载编排或 GPU Scene 提升为公共 SDK。
- 不把 Desktop 强行接入单线程 `application`，也不为一个消费者扩大通用 Host 的职责。
- 不合并 Desktop C++ RAII 渲染路径与 Web C ABI 验收路径；二者用于验证不同公共使用面。
- 不删除 Desktop 渲染线程，不把 Web 改成 pthread 或模拟异步渲染线程。
- 不重命名或移动 `examples/common/model_viewer`；它是教程和 Sample 共用的模型渲染能力。
- 不在本任务增加编辑器、多视口、热重载或新的 Viewer 功能。

## 线程与所有权约束

```text
Desktop 主线程
  Window / Input / ImGui / CPU 加载编排
          │ immutable frame packet / ordered command
          ▼
Desktop 渲染线程
  GPU 上传 / Pipeline / Swapchain 帧 / GPU 资源销毁

Web 浏览器主线程
  Host / Input / Asyncify 加载 / inline frame execution / JavaScript exports
```

- Window System、Window 和 ImGui Context 只在 Desktop 主线程访问。
- Desktop Frame Packet 在提交后不可变并拥有其数组及 UI Canvas 数据；主线程不能借出下一帧会修改
  的内存。
- 普通帧在执行前可以被更新帧替换；上传、质量修改、材质修改、重建和销毁命令不可替换且必须返回
  完成回执。
- 跨线程命令的上下文必须活到对应完成回执，不能依赖入口函数中的隐式临时对象生命周期。
- Web 所有状态继续位于浏览器主线程；`emscripten_sleep(0)` 只在允许 Asyncify 的加载和预热边界
  让出事件循环。
- Surface 创建需要 Window 创建线程时，Desktop 先停止或清空渲染任务，再按显式交接顺序重建，
  不允许两个线程同时修改呈现资源。

## 目标结构

目录层级保持不变，仅在现有 Desktop/Web 目录增加按职责命名的实现文件：

```text
examples/samples/model_viewer/
├─ application_core.*            # Viewer 状态与纯渲染帧生成
├─ frame_executor.*              # inline/threaded 调度机制
├─ viewer_input_accumulator.*    # Viewer 输入策略
├─ desktop/
│  ├─ main.cpp                   # composition root
│  ├─ application.*              # 主线程生命周期和状态推进
│  └─ render_service.*           # 渲染线程资源及有序命令
└─ web/
   ├─ application.*              # Host 与浏览器运行时
   ├─ pipeline_validation.*      # C API Pipeline 预热/生命周期验收
   └─ browser_api.cpp            # 稳定 JavaScript 导出转发
```

文件名是实施起点；若现有类型边界表明更准确的名称，可以在本计划范围内调整，但不新增更深目录或
移动 `common/model_viewer`。

## 实施顺序

1. **S-57A 帧契约分层（已完成）**：`application_core` 输出不再包含 ImGui 类型，`viewer_frame`
   保存纯渲染数据；执行层 `frame_packet` 再组合渲染帧与可选 UI Canvas。Desktop、Offscreen、Web
   和 Executor 已迁移，并通过 Windows 测试、桌面 smoke、Emscripten 构建与 Chrome 验收。
2. **S-57B Desktop 渲染服务**：把 GPU 上传、Pipeline 创建/替换、材质更新、Swapchain 帧执行、
   指标查询和有序销毁收进明确的渲染服务；服务内部使用 threaded executor，入口不再维护成组裸
   Context 结构。不可丢弃命令的“提交、刷新、定位自身回执”已封装为 executor 的同步操作，
   且不会消费其他命令的完成回执。`desktop/render_service.*` 已统一 GPU 上传、Pipeline
   替换、材质更新、Swapchain 重建、帧队列和有序销毁；下一步把 Renderer、Swapchain 及 UI GPU
   资源的所有权从入口转入服务，并单独保留 Surface 的主线程交接。
3. **S-57C Desktop 应用壳**：把加载阶段、窗口事件、UI 帧、呈现恢复和性能采样整理为可测试的
   Desktop application；`main.cpp` 只装配 options、application 并返回运行结果。保留直接 Window
   循环，不扩展通用 Application Host。
4. **S-57D Web 职责拆分**：将 Pipeline 预热和公共 C API 生命周期验收移出运行时文件，将
   JavaScript 导出集中为只校验参数并转发状态/控制的边界；Host 和 inline 帧执行行为不变。
5. **S-57E 重复审计**：完成拆分后再比较 Desktop/Web 的加载阶段、质量设置和呈现恢复。只有存在
   相同所有权与失败语义的逻辑才提升到 Sample Core；不为减少行数制造跨平台虚基类。
6. **S-57F 验证与文档**：补齐状态转换、线程命令、失败回滚和重复 Shutdown 测试，运行 Desktop、
   Emscripten、浏览器与安装边界检查，并更新 Model Viewer 指南及实施记录。

## 验收标准

- `application_core` 不包含 ImGui 类型，Viewer Core 测试无需 UI Integration；
- Desktop 渲染资源的创建、修改、帧执行和销毁路径能从单一服务接口审计；
- `main.cpp` 不再实现加载算法、渲染命令或帧录制，只保留进程级装配；
- Web 产品运行时、Pipeline 验收和 JavaScript 导出位于不同编译单元；
- Desktop 帧替换、跳帧、输入合并、质量修改、Surface/Swapchain 恢复和性能采样结果不变；
- Web 进度、取消、质量/光照控制、Resize、资源释放和公开导出名称不变；
- PBR 教程继续只依赖 `granit_example_model_scene`，不依赖完整 Viewer Sample；
- 示例私有目标不进入安装导出，公共 API、ABI 和 CMake component 不发生变化；
- Windows shared/static、Linux、Emscripten 和 Chrome 相关测试通过；
- Documentation 检查与 `git diff --check` 通过。

## 风险

- 把上下文结构机械包进类但仍由入口持有全部资源，只会隐藏耦合；每个提取步骤必须同时明确所有权
  和调用线程。
- Surface 创建受 Window 线程约束，不能简单把全部呈现资源移动到后台线程；重建协议必须保留显式
  主线程与渲染线程交接。
- Web 文件中的 Pipeline 预热同时承担真实启动工作和公共 C API 验收，拆分编译单元不能把它误删成
  仅测试代码。
- Desktop 与 Web 的差异来自线程和 C/C++ API 验收目标；追求实现文本一致会损害现有覆盖价值。
