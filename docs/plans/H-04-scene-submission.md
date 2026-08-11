<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-04：场景提交与可见性输入适配层

## 元数据

- 设计状态：已确认首版边界
- 实现状态：待实现
- 路线图任务：H-04
- 优先级：P2
- 前置依赖：H-01、H-02、H-03
- 后续依赖：H-05

## 目标

H-04 在核心 Renderer、材质模块、PBR 参考模块与 Render Graph 之上提供可选的逐帧场景提交适配层。
调用方显式提交 View、可渲染项、包围体和光源；适配层校验并复制值数据，执行基础 CPU 可见性筛选，
生成确定性的可见列表和 PBR Pass 输入。

该模块不是 ECS、Scene Graph 或游戏对象框架，不管理 Transform 层级、资产数据库、动画、窗口或 GPU
资源生命周期。核心 `granit` 不得反向依赖 H-04。

## 首版边界

### 包含

- 一个或多个显式 View，每个 View 包含 view/projection、相机位置、viewport 和可见层掩码。
- 静态可渲染项：对象标识、Model/Normal Matrix、世界空间包围球、层掩码和调用方 payload。
- 单方向光兼容路径，以及显式的方向光、点光和聚光输入数组。
- 基于 View Frustum 与包围球的 CPU 粗粒度筛选。
- 稳定、确定性的可见列表和排序键，为调用方录制 Draw 提供顺序。
- 将单方向光场景转换为 H-03 `pbr_graph_pass_desc` 的适配路径。
- 多 View、无可见对象、非法矩阵/包围体、层掩码和稳定排序测试。

### 不包含

- Entity/Component 存储、父子 Transform、脏标记传播和对象查询语言。
- Mesh、Material、Texture 或 Shader 资产所有权及文件加载。
- 遮挡剔除、Hi-Z、Portal、LOD、实例合并和 GPU Driven 绘制。
- 阴影图、IBL、反射探针、聚簇光照和最终光源 Buffer；这些由 H-05 消费 H-04 输入后实现。
- 自动创建窗口、Swapchain、Renderer 或 Render Graph 附件。

## 模块与依赖方向

首版建立内部静态目标 `granit::scene`，源码位于 `src/scene`，测试位于 `tests/scene`。模块暂不安装
导出，也不进入核心动态库：

```text
granit::scene
  -> granit::pbr
  -> granit::render_graph
  -> granit::material
  -> granit::granit
```

依赖只能自上而下。PBR、Material、Render Graph 和核心 Renderer 不得包含 Scene 头文件。

## 输入与所有权契约

- 提交 API 只在调用期间读取输入 span；成功后生成独立的帧快照，不保留调用方容器引用。
- 矩阵、包围体、光源和标识是值数据，不转换为整数句柄。
- `payload` 是调用方定义的 64 位不透明值，只用于把可见项映射回 Mesh/Material/Draw 数据；模块不
  解引用、不销毁，也不把它当作可持久化身份。
- GPU Buffer、Texture、Material Instance 等资源仍由调用方持有。H-04 不延长其生命周期；真正
  录制命令后由 Recorder 按现有规则保留底层资源。
- 帧快照不可跨线程修改；构建完成后的只读查询允许并发。首版不在模块内部创建线程。

## 坐标与可见性约定

- 沿用 H-03 的右手坐标与列主序矩阵；clip-space 深度范围为 Vulkan 的 `[0, 1]`。
- 调用方显式提供 view、projection 和 view-projection，H-04 校验有限值但不猜测矩阵来源。
- 首版包围体使用世界空间包围球；半径必须有限且非负，零半径表示点。
- Frustum 从 view-projection 提取六个归一化平面。球体与平面相交视为可见，避免边界闪烁。
- View 与对象层掩码按位相交；结果为零时无需进入 Frustum 测试。
- 首版只做粗粒度 CPU 筛选，不声称替代 Meshlet、GPU culling 或遮挡剔除。

## 光源契约

- 方向光使用“指向光源的方向 + 线性 RGB radiance”，与 H-03 完全一致。
- 点光使用世界位置、线性 RGB intensity 和有限正作用半径。
- 聚光在点光字段上增加归一化方向、内外锥角；必须满足 `0 <= inner <= outer < pi/2`。
- H-04 只校验、复制并按 View 层掩码筛选光源，不决定衰减公式、阴影或聚簇布局。
- H-03 的单方向光常量布局保持不变。多光源结果作为独立列表交给 H-05，不塞入 Material Group 1。

## 可见列表与排序

首版输出每个 View 对应的连续可见项索引数组。默认顺序使用稳定键：

1. 调用方 `sort_key`；
2. `object_id`；
3. 原提交顺序。

模块不从 Material 或 Pipeline 内部状态猜测排序键。调用方可将 pass、pipeline、material 和 depth
分桶编码进 `sort_key`；透明物体排序暂不属于首版。

## 分阶段实施

1. **H-04A**：定义无 Vulkan 类型的 View、包围球、Renderable、Light 和帧快照值模型，完成有限
   值、范围、标识与层掩码校验。
2. **H-04B**：实现 Vulkan `[0, 1]` clip-space Frustum 提取、包围球测试、层掩码筛选和稳定排序。
3. **H-04C**：支持多 View 帧快照和方向光/点光/聚光的逐 View 筛选，不建立 Scene 全局单例。
4. **H-04D**：将单方向光可见结果适配为 H-03 PBR Render Graph Pass；实际 Mesh/Material Draw 仍由
   调用方回调录制。
5. **H-04E**：增加多 View 端到端测试、生命周期测试和 100/1,000/10,000 对象 CPU 性能基线，
   整理 H-05 的光源、阴影和环境输入契约。

## 验收标准

- `include/granit/renderer` 与核心动态库不新增 Scene、Camera、Light 或 Vulkan 泄漏。
- 同一输入生成逐字节稳定的可见索引与排序结果。
- 多 View 之间没有隐式全局 Camera，调用方可独立提交不同 viewport 与层掩码。
- 输入容器释放或修改后，已构建帧快照仍然有效；快照销毁不影响调用方 GPU 资源。
- Frustum 边界、相交、完全外部、零半径、非法矩阵和非有限值均有测试。
- 10,000 对象基线建立前不引入任务系统、空间树、SIMD 专用路径或 GPU culling。
- H-04 不实现阴影、IBL、Tone Mapping 或完整多光源 Shader，不提前混入 H-05。

## 首个实现切片

H-04A 先只实现纯 CPU 值模型与单 View 快照构建器。Renderable 的 `payload` 保持不透明，避免在 Mesh
和资产模块尚未设计时绑定临时资源结构。此切片完成后再实现 Frustum 数学，不同时引入 Scene Graph、
空间索引或线程抽象。
