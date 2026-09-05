<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录记录单项路线图任务的实施设计，回答“具体如何实现”。计划允许在验证过程中修改，不代表
已经实现的公共能力，也不自动构成稳定 API/ABI 承诺。

## 与其他文档的关系

- 根目录 `README.md`：项目定位、快速开始和文档入口。
- `docs/README.md`：完整文档分类和推荐阅读顺序。
- `docs/concepts/architecture.md`：长期有效的架构边界和已经确认的决策。
- `docs/roadmap.md`：阶段、优先级、依赖关系和状态摘要。
- `docs/plans/*.md`：单项任务的 API 草案、内部设计、步骤、测试和未决问题。
- 根目录 `DOCUMENTATION_GUIDE.md`：所有文档类型、模板和生命周期的统一规范。

计划中经过实现验证且长期有效的结论，应同步回架构或对应功能文档。计划完成后保留记录并补充
最终差异；已经失效且不再具备参考价值的草案可移动到 `archive/`，不直接删除历史依据。

## 命名规则

文件名使用路线图中的稳定任务编号和简短英文主题：

```text
R-01-memory-allocation.md
R-02-resource-model.md
R-03-buffer.md
F-01-command-recorder.md
D-03-graphics-pipeline.md
```

## 当前计划

- [R-01：GPU 内存分配方案](R-01-memory-allocation.md)——已完成基础接入。
- [R-02：第一版资源模型](R-02-resource-model.md)——已完成。
- [R-03：Buffer 生命周期与映射](R-03-buffer.md)——已完成。
- [R-04：Buffer 初始数据与同步上传](R-04-buffer-upload.md)——已完成。
- [R-05：Texture 与 Texture View 生命周期](R-05-texture-view.md)——已完成。
- [R-06：Sampler 生命周期与能力限制](R-06-sampler.md)——已完成。
- [R-07：Swapchain Backbuffer 资源接入](R-07-swapchain-backbuffer.md)——已完成。
- [V-01：资源生命周期验证与诊断](V-01-lifetime-validation.md)——已完成。
- [R-08：GPU 资源延迟销毁基础](R-08-deferred-destruction.md)——普通资源真实完成点与 Swapchain
  presentation 安全退役均已完成。
- [R-09：统一 Render Target Attachment](R-09-render-target-attachment.md)——已完成。
- [R-10：通用资源传输](R-10-resource-transfer.md)——已完成同步回读、显式复制、子资源状态跟踪和
  mipmap 生成；异步 Readback 与多区域批量入口等待真实需求。
- [R-10C：Mipmap 生成](R-10C-mipmap-generation.md)——已完成子资源跟踪、能力门禁、公共命令及
  非二次幂、Cube 数组层和失败路径验证。
- [F-01：Command Recorder 基础](F-01-command-recorder.md)——已完成。
- [F-02：基础命令录制](F-02-command-recording.md)——Buffer 命令与 Dynamic Rendering 已完成。
- [F-03：帧同步内部抽象](F-03-frame-synchronization.md)——Fence、二进制 Semaphore 和每帧上下文
  已完成，等待 F-04 接入提交调度。
- [F-04：Queue 提交与 frames-in-flight](F-04-queue-submission.md)——异步提交、帧槽轮转和 Queue
  串行化已完成。
- [F-05：资源状态跟踪与屏障](F-05-resource-state-tracking.md)——Buffer 屏障和 Attachment Layout
  提交顺序解析已完成。
- [F-06：Swapchain 帧循环](F-06-swapchain-frame-loop.md)——Frame 令牌、acquire、Semaphore 提交链
  与 present 已完成。
- [F-07：窗口帧恢复边界](F-07-recovery-boundaries.md)——Frame 回收、零尺寸、Surface Lost 与
  Renderer 全局 Device Lost 门禁已完成。
- [F-10：公共帧上下文与 Recorder 轮转](F-10-public-frame-context.md)——已完成。
- [F-11：Canvas 绑定缓存与多纹理录制](F-11-canvas-binding-cache.md)——已完成；跨平台 CI 已通过。
- [F-12：帧循环性能诊断与提交优化](F-12-frame-loop-performance.md)——已完成；全帧基线与
  Validation 归因表明生产路径暂无通用提交或 Swapchain 调度改造依据。
- [D-01：Shader 输入与离线编译策略](D-01-shader-input.md)——运行时 SPIR-V 输入、离线工具、
  反射边界和错误语义已确认。
- [D-02：Shader Module 生命周期](D-02-shader-module.md)——SPIR-V 校验、Shader 句柄、Vulkan
  Module 和 RAII 已完成。
- [D-03：Graphics Pipeline 与 Bind Group](D-03-graphics-pipeline.md)——已完成。
- [D-05：基础绘制命令](D-05-draw-commands.md)——已完成。
- [D-06：基础渲染示例](D-06-examples.md)——已完成。
- [D-07：Compute Pipeline 与 Dispatch](D-07-compute-pipeline.md)——已完成。
- [D-08：Graphics Pipeline 完整状态、缓存与重载边界](D-08-pipeline-production.md)——设计已确认。
- [D-09：Bindless Resource Table 边界](D-09-bindless-resource-table.md)——草案；H-02E 只预留绑定
  模型，完成真实场景测量后再实现 Renderer 能力。
- [D-10：动态 Uniform Buffer Offset](D-10-dynamic-uniform-buffer-offsets.md)——已完成；动态绑定、
  公共设备限制查询、像素 Smoke Test 与跨平台矩阵均已验收。
- [P-01：并行录制、资源创建与上传压力测试](P-01-parallel-recording.md)——已完成。
- [P-02：CPU 并发与资源管理性能基线](P-02-performance-baseline.md)——已完成。
- [P-03：锁竞争归因与批量 API 优化](P-03-contention-and-batching.md)——已完成。
- [P-04：持久化上传分配器与批量上传](P-04-upload-allocator.md)——已完成。
- [P-05：线程池与外部执行器边界](P-05-executor-boundary.md)——已完成；当前不引入线程池或
  公开执行器 API。
- [P-06：Render Graph 职责与模块边界](P-06-render-graph-boundary.md)——已完成；功能实现转入
  H-01。
- [H-02：材质参数、Shader 变体与离线构建](H-02-material-system.md)——已完成内部原型；包含
  离线包、参数/资源实例、Pipeline 缓存、热替换、错误材质回退和性能基线，尚未安装导出。
- [H-02E3：持久化材质包格式](H-02-material-package-format.md)——已完成；包含确定性容器、
  SHA-256、材质语义往返、源 JSON 构建、调试导出和损坏输入防护。
- [H-03：金属度/粗糙度 PBR 渲染模块](H-03-pbr-renderer.md)——已完成；包含 Pipeline 状态、
  CPU BRDF、默认纹理、显式 Draw 输入、Render Graph 适配、生命周期/性能基线和 GPU 像素回归。
- [H-04：场景提交与可见性输入适配层](H-04-scene-submission.md)——已完成；包含快照、Frustum、
  多 View、光源筛选、PBR Render Graph 适配、生命周期验证和性能基线。
- [H-05：光照与后处理参考管线](H-05-lighting-pipeline.md)——已完成；包含多光源、阴影、IBL、
  HDR/Tone Mapping、性能基线、降级组合、窗口、多 View、统一 Render Graph、2,000 帧生命周期和
  跨平台安装 Consumer 验证。
- [H-06：Unlit、2D 与 UI 渲染路径](H-06-unlit-2d-ui.md)——已完成内部技术路线验证；公共 UI、
  Debug Draw 与 Text ABI 后续分别设计。
- [H-07：高级参考渲染套件](H-07-reference-render-pipeline.md)——已完成；公共 ABI、自动 Draw、
  离屏/窗口用户路径、安装 component、输出与性能验收均已闭合。
- [H-08：公共 UI、Debug Draw 与 Text components](H-08-ui-debug-text-components.md)——已完成；公共
  Canvas、Debug Draw、R8 Text Atlas、Pipeline 自动录制和第三方 Adapter 边界均已验收。
- [H-09：高级渲染能力真实负载评估](H-09-advanced-rendering-evaluation.md)——已完成；四项能力均按
  独立证据暂缓原型，详见[完成记录](../records/H-09-advanced-rendering-evaluation.md)。
- [H-09B：透明 PBR 正确性草案](H-09B-transparent-pbr-correctness.md)——已确认评估契约；现有基线和
  产品需求不足以支持进入原型，暂时保留 Unlit 透明路径。
- [S-01：C ABI 回归验证](S-01-abi-regression.md)——已完成 Core、RenderPipeline、Window 与 Input
  的版本化正式快照、共享库导出和布局回归。
- [S-02：统一诊断、GPU 调试名称与 Device Lost 报告](S-02-diagnostics.md)——已完成；统一诊断
  sink、公共回调、GPU 调试名称、首次 Device Lost 报告及边界回归均已落地。
- [S-03：安装包与外部 Consumer 验证](S-03-package-consumers.md)——已完成；Windows/Linux
  共享与静态安装 Consumer、版本选择和导出审计均已通过 CI。
- [S-04：Linux XCB 与 Wayland Surface](S-04-linux-surface.md)——实现已完成；XCB、Wayland、窗口
  示例和无头集成测试已通过 Linux GCC/Clang 共享与静态运行矩阵。
- [S-06：版本与兼容承诺](S-06-compatibility-policy.md)——0.x 策略、component 契约、变更记录和
  发布验收准备已完成；最终稳定承诺等待版本与 component 范围决策。
- [S-07：Window、Event 与 Input 边界](S-07-window-events.md)——Win32、XCB 与 Wayland Window、
  统一事件及 Renderer 集成均已实现并通过跨平台 CI。Renderer Surface 仍允许 SDL、
  GLFW、Qt 和引擎直接接入。
- [S-07E：Input component 边界](S-07E-input-component.md)——已完成；独立可选组件、Window 内部
  事件分发桥、键盘、指针和文本路径均已通过跨平台 CI。
- [S-08：SDL3 与 ImGui 第三方集成](S-08-third-party-integrations.md)——已完成；Win32 及 Linux
  X11/Wayland 共享与静态运行矩阵均已通过，第三方依赖不进入核心。
- [S-09：0.3.0 公共 SDK 易用性与集成体验](S-09-0.3.0-sdk-usability.md)——已完成；公共使用路径、
  契约、诊断、安装 Consumer、迁移说明和 Release 预验证均已验收。
- [S-10：0.4.0 多后端与 WebGPU](S-10-0.4.0-webgpu-backend.md)——已完成；统一 Registry、私有
  HAL 与浏览器闭环均已验收；其中桌面 WebGPU 原型后来由 S-16 移除。
- [S-10C：WGSL Shader 工具链](S-10C-wgsl-toolchain.md)——已完成；以 WGSL 为源码权威，已形成
  SPIR-V、反射清单、确定性资产、诊断和缓存闭环。
- [S-12：WebGPU 公共能力补齐](S-12-webgpu-feature-parity.md)——已完成；公共后端选择、模型绘制
  所需资源与命令已验收；当前共同 Fixture 覆盖桌面 Vulkan 和浏览器 WebGPU。
- [S-13：跨后端模型查看器](S-13-cross-backend-model-viewer.md)——已完成；示例私有 glTF 加载器、
  PBR 头盔、轨道相机、ImGui、三个运行目标及跨后端截图/性能验收均已闭合。
- [S-13H：模型查看器环境光照](S-13H-model-viewer-environment-lighting.md)——已完成；复用 Group 3
  IBL 布局，为模型查看器补齐可再分发的摄影棚环境与跨后端验收。
- [S-13I：跨后端渲染质量配置](S-13I-render-quality.md)——已完成；统一 MSAA、FXAA、Specular AA、
  Mipmap 与各向异性过滤的能力查询、公开选项和跨后端验收。
- [S-15：私有 HAL 结构整理](S-15-internal-hal-structure.md)——已完成；集中后端能力发现与
  Registry 依赖，并整理 HAL 契约和具体后端目录。
- [S-16：WebGPU 收敛到 Emscripten 浏览器](S-16-browser-only-webgpu.md)——已完成；删除桌面
  Dawn、动态 Provider 与 SDK/CI 成本，保留统一 HAL 和浏览器 WebGPU。
- [S-17：Render Pipeline 内部职责收敛](S-17-render-pipeline-internal-structure.md)——已完成；
  拆分提交数据、Forward/Shadow/Tone Mapping 录制与 GPU 指标管理，公共 API/ABI 保持不变。
- [S-18：0.5.0 平台扩展与上游集成](S-18-0.5.0-platform-upstream-integration.md)——已完成；真实
  Consumer 基线、窗口状态、资源契约和 SDK 验收均已闭环，Android 延期。
- [S-19：Model Viewer 渲染线程](S-19-model-viewer-render-thread.md)——已完成；桌面使用有界渲染
  线程和异步上传，浏览器保持同步执行。
- [S-20：Shader Asset 后端变体](S-20-shader-asset-variants.md)——已完成；后端 sidecar、能力选择和
  WGSL/HLSL/GLSL portable 前端已形成闭环。
- [S-21：可复现 Shader Toolchain 包](S-21-shader-toolchain-package.md)——已完成；发布带版本、许可
  和 SHA-256 清单的离线编译工具包，并让官方 CI 使用严格锁定策略。
- [S-22：0.7.0 SDK 稳定化与上游集成](S-22-0.7.0-sdk-stabilization.md)——S-22A component 契约
  清单、安装 SDK 门禁、上游边界审计和发布候选验收均已完成。
- [S-23：0.8.0 运行时 Shader 与材质资产契约](S-23-0.8.0-runtime-shader-assets.md)——已完成；
  `.grshader` 已进入 Core，`.grmat` 已引用同一 Shader Asset 身份，发布矩阵与产物预验证均已通过。
- [S-24：0.9.0 公共 PBR 与渲染资产收敛](S-24-0.9.0-public-pbr-assets.md)——已完成；
  公共 PBR Shader、四组 Binding、安装资产与 Model Viewer Consumer 已统一。
- [S-25：0.10.0 环境资源与帧构造背压](S-25-0.10.0-environment-and-frame-backpressure.md)——
  已完成；Environment Map、GRENV v3、Model Viewer 迁移与构造前背压已通过发布验收。
- [S-26：0.11.0 WebGPU 能力对齐与浏览器 Model Viewer](S-26-0.11.0-webgpu-parity-and-web-model-viewer.md)——
  已确认；将补齐可移植传输、Mipmap、Timestamp，并交付正式浏览器 Model Viewer。

## 状态

- **草案**：仍有影响实现方向的未决问题。
- **已确认**：主要设计已经同意，可以进入实现。
- **实现中**：代码和测试正在落地。
- **已完成**：验收通过，并记录最终实现差异和提交。
- **已暂停**：存在明确阻塞或优先级调整，必须写明原因。

计划结构、ADR/Guide 模板、篇幅和拆分规则统一遵循
[项目文档规范](../../DOCUMENTATION_GUIDE.md)。草案代码只用于表达接口方向，不保证与最终实现
逐字一致；文档不得把计划能力描述为仓库当前已经具备的功能。
