<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-06：Render Graph 职责与模块边界

## 元数据

- 设计状态：已确认
- 实现状态：已完成（本任务只完成架构评估，功能实现转入 H-01）
- 路线图任务：P-06
- 优先级：P2
- 前置依赖：P-01～P-05、F-02、F-04、F-05
- 后续实现：H-01

## 决策

Render Graph 不进入 Granit 核心 Renderer，也不改变现有显式命令 API。它作为可选高层模块，
建立在 Buffer、Texture、Texture View、Pipeline、Command Recorder 和提交接口之上。使用者始终
可以绕过 Render Graph，直接使用底层 Renderer。

P-06 不发布公共 API。H-01 首版采用调用者线程上的确定性串行编译与执行，先验证依赖表达、
Pass 裁剪、资源状态和生命周期；没有测量证据前不加入内部线程池、并行录制、异步计算或瞬态
资源内存别名。

## 职责

Render Graph 负责：

- 声明 Pass 对逻辑 Buffer、Texture 和 Attachment 的读写用途。
- 根据资源生产者与消费者建立有向无环图，并拒绝循环依赖和未初始化读取。
- 从导出资源和具有副作用的 Pass 反向裁剪无效工作。
- 计算 Pass 的稳定拓扑顺序和逻辑资源的首末使用区间。
- 创建、保活并在执行完成后释放图内瞬态资源。
- 把声明的资源用途交给现有资源状态跟踪和 Command Recorder 执行。
- 提供包含 Pass 名称、依赖边和逻辑资源生命周期的诊断信息。

Render Graph 不负责：

- Scene、Entity、Camera、Light、材质系统、资产加载或 Shader 编译。
- 窗口事件循环、Swapchain 重建策略或 Renderer 生命周期。
- 隐式持有跨帧资源；历史缓冲等持久资源必须由调用者创建并导入。
- 替代底层句柄验证、资源状态跟踪、延迟销毁和 Queue 同步。
- 自动创建线程、调度任意用户任务或隐藏长时间 GPU 等待。

## 资源模型

图内资源使用只在一次图构建和执行期间有效的逻辑标识，不直接复用公开 GPU 资源句柄：

- `imported`：由调用者拥有的 Buffer、Texture View 或 Swapchain Backbuffer；图只借用。
- `transient`：由图描述并在执行时创建；首版不做跨资源内存别名。
- `exported`：执行后需要被调用者观察的输出，也是裁剪根节点。

导入资源必须声明进入图时的预期用途和执行后的目标用途。Swapchain Backbuffer 仍通过现有
acquire/present 流程进入和离开图，Render Graph 不获得其所有权。逻辑标识在 reset、编译失败或
执行结束后失效，不允许持久化或跨图混用。

## Pass 模型

每个 Pass 至少包含名称、类型、资源访问声明和录制回调。首版类型只需覆盖 graphics、compute
和 copy，并全部提交到当前 Renderer 支持的同一有序 Queue。

访问声明必须区分读取、写入和读写，并表达采样、Storage、Attachment、复制源/目标等实际用途。
同一 Pass 内存在冲突或无法表达的反馈环时应在编译阶段失败，不能依赖添加顺序猜测意图。

默认仅从资源依赖推导顺序。具有外部可见行为但不产生导出资源的 Pass 必须显式标记副作用，
否则允许被裁剪。拓扑排序在多个合法结果间保持声明顺序，以便测试、诊断和帧捕获可复现。

## 与底层状态和同步的关系

Render Graph 只声明“需要怎样使用资源”，不建立第二套 Vulkan Layout 或 barrier 状态机。编译结果
按顺序调用现有 Command Recorder，由 Renderer 的资源状态跟踪生成实际转换；图层可提前验证
明显冲突，但底层仍是句柄归属、状态和同步正确性的最终防线。

首版使用单次编译、单线程录制和有序提交，不提供跨 Queue ownership transfer。未来多 Queue 或
并行录制必须基于 profiler 数据单独设计，并继续遵守 P-05 的外部执行器边界。

## 错误与生命周期

- 构建错误、循环依赖、非法资源访问和创建失败通过结果码返回，不让异常跨越 C ABI。
- 编译失败时不得执行任何 Pass，也不得遗留已创建的瞬态 GPU 资源。
- Pass 录制失败后停止后续录制，并按底层提交完成点安全回收已创建资源。
- 回调参数只在回调期间有效；不得保存内部对象或在 Renderer 销毁后继续调用。
- Renderer Device Lost、Swapchain 过期和最小化仍沿用现有恢复边界，不由图层吞掉或重试。
- 调试构建应报告 Pass 名称和逻辑资源名称，但名称不参与依赖或正确性判断。

## H-01 最小实现顺序

1. **H-01A：纯 CPU 图编译器（已完成）**——逻辑资源、依赖构建、循环检测、稳定拓扑排序和
   裁剪。当前仅由独立测试目标构建，不进入核心 `granit` 动态库。
2. **H-01B：导入资源与串行执行（已完成）**——已接入 Buffer、Texture View 和单个
   Command Recorder 的串行执行原型。
3. **H-01C：瞬态资源生命周期**——按首末使用创建和回收独立资源，不做内存别名。
4. **H-01D：窗口输出与诊断**——接入 Backbuffer、错误上下文和可导出的图结构。
5. **H-01E：性能复核**——测量编译、录制、提交及资源创建成本，再决定缓存、池化或并行化。

## 后续重新评估项

以下能力不属于首版，只有基准或真实场景证明需要时才立项：

- 图结构缓存和跨帧编译结果复用。
- 瞬态 Buffer/Texture 池与内存别名。
- 多 Queue、异步 Compute 和 Queue 间同步。
- 基于外部执行器的并行 Pass 录制。
- Pass 合并、自动 resolve 和子资源粒度更细的依赖分析。

## H-01 验收基线

- 纯 CPU 测试覆盖线性、分叉、汇合、循环、未初始化读取、稳定排序和无效 Pass 裁剪。
- 集成测试覆盖离屏 graphics、compute 到 graphics、copy 和 Backbuffer 输出。
- 编译或执行失败不泄漏逻辑资源、GPU 资源或 Command Recorder。
- Validation Layer 下无生命周期、同步或 Layout 错误。
- 不使用 Render Graph 的现有公开 API、示例和性能基线不发生行为变化。

## H-01A 实现记录

H-01A 已加入内部 `graph_compiler` 原型及独立 Catch2 测试。原型支持 imported/exported 逻辑资源、
读写依赖、显式 Pass 依赖、副作用根节点、反向裁剪、稳定拓扑排序和资源首末使用区间，并报告
越界资源、重复访问、未初始化读取和依赖环。

该原型尚无公共 C/C++ API，不参与安装导出，也不链接到核心动态库。内部类型仍可根据后续执行
接口需要调整，不构成 API 或 ABI 承诺。

## H-01B 实现记录

H-01B 已加入内部 `serial_graph` 原型。调用者可导入非零 Buffer 或 Texture View 句柄，并为 Pass
提供内部 C++ 录制回调。编译成功后，整张图创建并开始一个 Command Recorder，按稳定拓扑顺序
调用保留的 Pass，最后结束并提交一次；成功结果把 pending Recorder 交还调用者负责销毁。

Pass 上下文只解析当前 Pass 声明过且类型匹配的导入资源。回调为空、返回失败或抛出异常时立即
停止后续 Pass，销毁未提交 Recorder，并报告失败阶段和 Pass ID。图编译失败或图被完全裁剪时
不会创建 Recorder。

该接口仍仅由独立测试目标构建，使用 `std::function` 也不会跨动态库 ABI。H-01C 加入瞬态资源
之前，需要进一步确定创建描述、资源与 View 的成对生命周期，以及执行失败后的逆序回收规则。

## P-06 完成条件

P-06 是架构评估任务。本文确认模块边界、首版范围、执行模型、生命周期和 H-01 分阶段顺序后即
视为完成；Render Graph 的公共接口和代码实现由 H-01 单独设计、测试和交付。
