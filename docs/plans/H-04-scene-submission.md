<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-04：场景提交与可见性输入适配层

## 状态

- 路线图任务：H-04
- 优先级：P2
- 状态：已完成
- 前置依赖：H-01 Render Graph、H-02 Material、H-03 PBR
- 后续依赖：H-05 Lighting、H-07 Render Pipeline
- 历史记录：[H-04 实施记录](../records/H-04-scene-submission-implementation.md)

## 目标与边界

H-04 提供可选的逐帧场景提交适配层。调用方显式提交 View、Renderable、包围体和光源；适配层
事务式校验并复制值数据，执行基础 CPU 可见性筛选，生成确定性的逐 View 可见列表和 PBR 输入。

该模块不是 ECS、Scene Graph 或游戏对象框架，不管理 Transform 层级、资产数据库、动画、窗口或
GPU 资源生命周期。核心 Renderer、Material、PBR 和 Render Graph 不得反向依赖 Scene。

## 输入模型

- View 包含矩阵、相机位置、viewport 和可见层掩码。
- Renderable 包含对象标识、Model/Normal Matrix、世界空间包围球、层掩码、排序键和 payload。
- 光源包含方向光、点光和聚光的显式值数组。
- `payload` 是调用方定义的 64 位不透明值，只用于映射 Mesh、Material 或 Draw 数据。
- 输入 span 只在调用期间读取；成功后 Snapshot 拥有独立复制，不保存调用方容器地址。
- GPU 资源仍由调用方持有，Scene Snapshot 不延长其生命周期。

## 可见性与排序

- 使用右手坐标、列主序矩阵和 `[0, 1]` clip-space 深度。
- 从 view-projection 提取六个归一化 Frustum 平面。
- 首版包围体为世界空间包围球；与平面相交仍视为可见。
- View 与对象层掩码按位相交，结果为零时直接排除。
- 每个 View 独立筛选 Renderable 和光源，不存在全局 Camera。

可见列表使用稳定顺序：

1. 调用方 `sort_key`。
2. `object_id`。
3. 原提交顺序。

模块不从 Material 或 Pipeline 内部状态猜测排序键，也不处理透明深度排序。

## 光源契约

- 方向光使用“指向光源的方向 + 线性 RGB radiance”。
- 点光使用世界位置、线性 RGB intensity 和有限正作用半径。
- 聚光额外包含归一化方向及合法的内外锥角。
- H-04 只校验、复制并按 View 层掩码筛选光源，不决定衰减、阴影或 Cluster 布局。
- 多光源结果作为独立逐 View 列表交给 H-05，不塞入 Material Group。

## 完成结果

H-04 已完成：

- 无 Vulkan 类型的 View、Renderable、包围球、光源和 Snapshot 值模型。
- Frustum 提取、包围球测试、层掩码筛选和稳定排序。
- 共享一份场景输入的多 View Snapshot 和逐 View 光源可见索引。
- 单方向光结果到 PBR Render Graph Pass 的适配路径。
- 多 View、生命周期和 100/1,000/10,000 对象 CPU 性能基线。

现有数据没有证明需要空间树、内部任务系统或 GPU Culling。逐阶段过程见
[H-04 实施记录](../records/H-04-scene-submission-implementation.md)。

## 非目标

- 不拥有 Mesh、Material、Texture、Shader 或资产文件。
- 不实现遮挡剔除、Hi-Z、Portal、LOD、实例合并或 GPU Driven 绘制。
- 不创建阴影图、IBL、最终光源 Buffer、窗口、Swapchain 或 Render Graph 附件。
- 不在模块内部创建线程池或全局 Scene 单例。

## 验收标准

- 同一输入生成稳定的逐 View 可见索引和排序结果。
- 输入容器释放后，成功构建的 Snapshot 仍然有效。
- 多 View 可以使用不同 viewport、矩阵和层掩码。
- 边界相交、完全外部、零半径、非法矩阵和非有限值均有测试。
- 性能优化必须由可复现基线触发。
