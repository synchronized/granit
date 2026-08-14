<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-06：Render Graph 职责与模块边界

## 状态

- 路线图任务：P-06、H-01
- 优先级：P2
- 状态：已完成内部原型
- 前置依赖：P-01～P-05、F-02、F-04、F-05
- 后续使用者：H-02～H-07
- 历史记录：[P-06/H-01 实施记录](../records/P-06-render-graph-implementation.md)

## 决策

Render Graph 是建立在资源、Command Recorder 和提交接口之上的可选高层模块，不进入核心
Renderer，也不改变现有显式命令 API。使用者始终可以绕过 Render Graph。

首版采用调用者线程上的确定性串行编译与单 Queue 执行。没有测量证据前，不加入内部线程池、
并行录制、异步 Compute、跨 Queue 同步或瞬态资源内存别名。

## 职责

Render Graph 负责：

- 声明 Pass 对逻辑 Buffer、Texture 和 Attachment 的读写用途。
- 根据生产者与消费者建立有向无环图，拒绝循环和未初始化读取。
- 从导出资源和副作用 Pass 反向裁剪无效工作。
- 生成稳定拓扑顺序和资源首末使用区间。
- 创建、保活和释放图内瞬态资源。
- 把用途需求交给现有 Renderer 状态跟踪和 Recorder 执行。
- 输出 Pass、依赖和逻辑资源生命周期诊断。

Render Graph 不负责：

- Scene、Camera、Light、Material、资产加载或 Shader 编译。
- 窗口事件循环、Swapchain 重建策略或 Renderer 生命周期。
- 隐式持有跨帧历史资源。
- 替代底层句柄验证、状态跟踪、延迟销毁和 Queue 同步。
- 自动创建线程或隐藏长时间 GPU 等待。

## 资源与 Pass 模型

逻辑资源只在一次图构建和执行期间有效：

- `imported`：调用方拥有的 Buffer、Texture View 或 Backbuffer，图只借用。
- `transient`：由图描述并在执行时创建，首版不做内存别名。
- `exported`：执行后需要观察的输出，也是裁剪根节点。

Pass 包含名称、资源访问声明和录制回调，首版覆盖 graphics、compute 和 copy，并提交到同一有序
Queue。访问必须区分读取、写入和读写，以及 sampled、storage、attachment、copy 等真实用途。

Render Graph 不建立第二套 Vulkan Layout 或 Barrier 状态机。编译结果按顺序调用现有 Recorder，
底层 Renderer 仍是句柄归属、资源状态和同步正确性的最终防线。

## 错误与生命周期

- 图构建、循环、非法访问和资源创建错误通过结果码返回。
- 编译失败时不执行 Pass，也不遗留瞬态 GPU 资源。
- Pass 录制失败后停止后续录制，并按提交完成点安全回收资源。
- 回调参数只在调用期间有效，不得保存内部对象。
- Device Lost、Swapchain 过期和最小化沿用 Renderer 的恢复边界。

## 完成结果

P-06/H-01 已完成：

- 纯 CPU 依赖编译、循环检测、稳定拓扑排序和 Pass 裁剪。
- Buffer、Texture View 和 Backbuffer 导入。
- Graphics、Compute、Copy Pass 的单 Recorder 串行执行。
- 按首末使用创建和回收瞬态资源。
- 图结构诊断、生命周期测试和首份性能基线。

当前测量不支持立即增加缓存、并行化或内存别名。详细过程与基准结论见
[P-06/H-01 实施记录](../records/P-06-render-graph-implementation.md)。

## 重新评估条件

只有基准或真实场景证明需要时，才分别立项评估：

- 图结构缓存和跨帧编译结果复用。
- 瞬态资源池与内存别名。
- 多 Queue、异步 Compute 和 Queue 间同步。
- 基于外部执行器的并行 Pass 录制。
- Pass 合并、自动 Resolve 和更细的子资源依赖。

## 验收标准

- CPU 测试覆盖线性、分叉、汇合、循环、未初始化读取和裁剪。
- 集成测试覆盖 Graphics、Compute、Copy 和 Backbuffer 输出。
- 编译或执行失败不泄漏逻辑资源、GPU 资源或 Recorder。
- Validation Layer 下无生命周期、同步或 Layout 错误。
- 不使用 Render Graph 的公共 Renderer 路径不发生行为变化。
