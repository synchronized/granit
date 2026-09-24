<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-59：Model Viewer 渲染任务运行时

## 状态

**本地实施完成，等待远端 Linux 与浏览器验收，P1。** 本计划延续 S-57 的运行时拆分和 S-58 的
统一资产入口，把只接收帧与无类型命令的 `frame_executor` 提升为强类型渲染任务执行器，并在其上
建立 Desktop/Web 共用的 Viewer 生命周期。
该能力仅服务仓库示例，不进入公共 SDK，也不改变 Granit API/ABI。

## 背景与目标

当前 Desktop 使用专用渲染线程执行帧、上传和配置命令，Web 则在浏览器主线程内联执行帧。两端已经
共用 Viewer Core、输入累积器、模型加载 Session 和资产系统，但仍分别编排 Renderer Ready、模型
上传、运行、失败恢复和退出。现有 `submit_command(callback, void*)` 缺少任务类型、数据所有权和
结果类型，继续用它承载生命周期只会把平台状态机藏进回调。

本阶段完成以下收敛：

- 将 `frame_executor` 改为拥有任务数据和完成结果的 `render_task_executor`；
- 明确帧可替换，上传、重配置、资源释放和停止任务不可丢弃且保持顺序；
- Desktop 使用有界线程执行策略，Web 使用浏览器主线程内联策略，两者保持相同任务语义；
- 提取 `model_viewer_runtime`，统一加载完成后的 GPU 上传、运行、失败和关闭状态转换；
- 让 Desktop/Web 入口只保留窗口、输入、平台异步读取和浏览器导出等真实平台差异。

## 非目标

- 不把执行器扩展成通用线程池，不调度文件读取、网络 Fetch、glTF CPU 导入或 Window 事件。
- 不把 Model Viewer 私有类型安装到 SDK，也不新增公共 Renderer 调度 API。
- 不删除 Desktop 渲染线程，不在 Web 中引入 pthread 或后台 WebGPU 设备访问。
- 不强制 Desktop/Web 使用相同的 Surface、Swapchain 或 ImGui 实现文本。
- 不在本阶段增加热重载、多视口或新的渲染效果。

## 已确认决策

### 任务边界

渲染执行器只接受需要访问 Renderer 或其资源的强类型任务：场景上传、帧执行、质量重配置、资源
释放和停止。任务拥有跨执行边界所需的数据，完成结果按任务序号返回，不暴露裸 `void*` 上下文。

普通帧允许被较新的帧替换；控制任务不可替换。执行器必须保证控制任务之间以及控制任务与其前后帧
之间的提交顺序。`flush` 只等待已经接受的任务，不隐式提交新工作。

### 执行策略

- `threaded_render_executor` 在 Desktop 专用渲染线程消费有界队列并实施帧背压；
- `inline_render_executor` 在 Web 浏览器主线程立即执行相同任务；
- 两种实现共享任务、完成、错误和停止协议，差异只限于任务在何处执行。

### 共享运行时

`model_viewer_runtime` 组合 `application_core`、`model_loading_session` 与渲染执行器，维护空闲、加载、
上传、运行、失败和停止状态。平台壳提供资产读取结果、输入和帧时机；运行时不拥有 Window、浏览器
Fetch、文件系统或 ImGui Context。

## 实施顺序

1. **S-59A 计划与契约基线（已完成）**：已登记任务类型、所有权、背压、完成结果和平台边界。
2. **S-59B 拥有型执行器（已完成）**：执行器、测试和构建入口已统一改名为
   `render_task_executor`；控制任务使用拥有捕获数据的 `std::function`，不再通过裸回调与 `void*`
   传递上下文，并保持现有帧替换、完成回执和同步等待行为。
3. **S-59C Desktop 迁移（已完成）**：上传环境与字体像素现在由任务侧数据拥有；Pipeline/质量修改、
   Swapchain 重建、材质更新与资源释放均通过拥有型控制任务执行，不再把调用栈临时上下文借给
   渲染线程。
4. **S-59D Web 迁移（已完成）**：Web 状态持久持有 inline 执行器，通过相同任务协议完成上传、帧、
   质量修改和资源释放，不再每帧临时构造执行器；Swapchain 信息也由运行状态持续维护。
5. **S-59E 共享 Viewer Runtime（已完成）**：新增 `model_viewer_runtime` 连接
   `model_loading_session` 与 `application_core`，统一 Renderer 阶段、资产轮询、CPU 导入、Scene/计划
   交接、失败同步、取消和重置；Desktop/Web 只保留各自的 I/O、窗口循环和 GPU 执行策略。
6. **S-59F 验收与文档（本地完成）**：Model Viewer Guide 已记录共享 Runtime、任务语义与平台
   边界；Windows Desktop、共享单测、Emscripten Web 构建和文档检查已经通过，等待远端 Linux 与
   浏览器行为验收。详细结果见
   [S-59 本地验收记录](../records/2026-09-24-s59-model-viewer-render-runtime-local-acceptance.md)。

## 测试与验收

- 单元测试覆盖任务顺序、帧替换、不可丢弃控制任务、完成结果、flush、停止和重复停止；
- inline/threaded 对相同任务序列产生等价的可观察结果；
- Desktop 模型加载、质量切换、Resize、Surface 恢复、退出和资源释放保持通过；
- Web 模型加载、取消、质量/光照控制、Resize、浏览器导出和资源释放保持通过；
- 平台入口不再直接编排加载完成后的公共 Viewer 状态转换；
- 示例私有目标不进入安装导出，公共 API/ABI 不变；
- Windows shared/static、Emscripten、Chrome、文档检查和 `git diff --check` 通过。

## 风险与未决问题

- `std::variant` 中放入大型帧数据可能造成额外移动；任务信封应拥有数据，但避免无意义复制。
- Desktop Surface 创建存在 Window 线程约束，类型化任务不能掩盖主线程交接协议。
- Web Fetch 和 Asyncify 不属于 GPU 执行器；共享 Runtime 只消费完成结果，不能阻塞浏览器事件循环。
- 共享状态机若开始处理平台 UI 或 Window 生命周期，应立即停止扩张并保留平台适配层。
