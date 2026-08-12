<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 开发计划文档

本目录记录单项路线图任务的实施设计，回答“具体如何实现”。计划允许在验证过程中修改，不代表
已经实现的公共能力，也不自动构成稳定 API/ABI 承诺。

## 与其他文档的关系

- 根目录 `README.md`：项目定位、当前能力和入口。
- `docs/architecture.md`：长期有效的架构边界和已经确认的决策。
- `docs/roadmap.md`：做什么、优先级、依赖关系和阶段验收标准。
- `docs/plans/*.md`：单项任务的 API 草案、内部设计、步骤、测试和未决问题。

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
- [H-04：场景提交与可见性输入适配层](H-04-scene-submission.md)——进行中；H-04A～H-04B 已完成
  单 View 快照、Frustum、层过滤和稳定可见列表，下一步进入 H-04C 多 View 与光源筛选。

## 状态

- **草案**：仍有影响实现方向的未决问题。
- **已确认**：主要设计已经同意，可以进入实现。
- **实现中**：代码和测试正在落地。
- **已完成**：验收通过，并记录最终实现差异和提交。
- **已暂停**：存在明确阻塞或优先级调整，必须写明原因。

## 推荐结构

每份计划按需要包含以下章节：

1. 状态、路线图任务、优先级和依赖。
2. 背景、目标与非目标。
3. 已确认决策与判断依据。
4. 公共 C API 和 C++20 包装草案。
5. 内部对象、所有权和生命周期。
6. 线程安全、锁和异步边界。
7. 错误处理与诊断。
8. 第三方依赖及 CMake 接入。
9. 分步实施顺序。
10. 测试矩阵和验收标准。
11. 风险、未决问题和后续任务。
12. 完成后的实际差异与相关提交。

草案代码只用于表达接口方向，不保证与最终实现逐字一致。文档不得把计划能力描述为仓库当前
已经具备的功能。
