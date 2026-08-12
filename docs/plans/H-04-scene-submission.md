<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-04：场景提交与可见性输入适配层

## 元数据

- 设计状态：已确认首版边界
- 实现状态：已完成（H-04A～H-04E）
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

1. **H-04A（已完成）**：定义无 Vulkan 类型的 View、包围球、Renderable、Light 和帧快照值模型，
   完成有限值、范围与层掩码校验。
2. **H-04B（已完成）**：实现 Vulkan `[0, 1]` clip-space Frustum 提取、包围球测试、层掩码筛选
   和稳定排序。
3. **H-04C（已完成）**：支持多 View 帧快照和方向光/点光/聚光的逐 View 筛选，不建立 Scene
   全局单例。
4. **H-04D（已完成）**：将单方向光可见结果适配为 H-03 PBR Render Graph Pass；实际
   Mesh/Material Draw 仍由调用方回调录制。
5. **H-04E（已完成）**：增加多 View 端到端测试、生命周期测试和 100/1,000/10,000 对象 CPU
   性能基线，整理 H-05 的光源、阴影和环境输入契约。

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

## H-04A 实现记录

新增内部静态目标 `granit::scene`。`frame_submission` 通过 span 接收单个 View、Renderable、方向光、
点光和聚光数组；`build_frame_snapshot` 事务式校验并复制为拥有数据的 `frame_snapshot`。失败不会
覆盖调用方已有快照，成功后原输入容器可立即修改或释放。

View 同时保存 view、projection 与 view-projection，避免模块猜测矩阵乘法来源。Renderable 保存
世界空间包围球、层掩码、稳定排序键、对象标识和 64 位不透明 payload。方向光与聚光方向在构建时
规范化；所有矩阵、向量、viewport、半径、radiance/intensity 与锥角均进行有限值和范围校验。

模块目前仅依赖核心 `granit` 目标，不包含 Vulkan、Material、PBR 或 Render Graph 头文件。H-04B
继续在该只读快照上增加 Frustum 与稳定可见列表，没有改变 H-04A 的所有权规则。

## H-04B 实现记录

`extract_frustum` 从列主序 view-projection 的四个行向量提取左右、上下、近远六个平面。近裁剪面
直接使用 row 2，远裁剪面使用 row 3 - row 2，明确匹配 Vulkan clip-space 的 `0 <= z <= w`；
平面统一规范化，退化矩阵返回 `invalid_frustum`。

`build_visible_list` 先按 View 与 Renderable 的 64 位层掩码相交结果过滤，再以世界空间包围球测试
六个平面。与平面相交的球体视为可见。输出保存原快照的 32 位 Renderable 索引，并按 `sort_key`、
`object_id` 排序；两者均相同时依靠稳定排序保留原提交顺序。构建采用事务语义，非法 Frustum 或
分配失败不会覆盖已有列表。

测试覆盖 Vulkan Z 范围、平面边界相交、左右/近远裁剪、层过滤、确定性排序和失败保留旧结果。
H-04C 下一步在同一值模型上支持多 View 帧快照和逐 View 光源筛选。

## H-04C 实现记录

`multi_view_submission` 接收 View 数组和一份共享的 Renderable/Light 数组；事务式构建成功后，
`multi_view_snapshot` 只复制一份共享场景值数据，每个 `view_visibility` 保存 View 值及四类 32 位
索引：Renderable、方向光、点光和聚光。输入容器可在构建后立即修改或释放，多 View 之间不存在
隐式全局 Camera 或可变共享状态。

每个 View 独立执行层掩码与 Frustum 筛选。方向光只按层筛选；点光以位置和作用半径构造包围球；
聚光首版也使用作用半径的保守包围球，不使用锥体做精细剔除，从而不会因为锥体近似错误而漏光。
光照衰减、锥角响应、阴影和聚簇分配仍由 H-05 决定。

构建拒绝空 View 数组、非法任一 View、退化 Frustum 和超过 32 位索引范围的输入；任意 View 失败
都不会覆盖已有多 View 快照。测试覆盖两个 View 的独立层掩码、Frustum 外光源、共享数据所有权、
方向规范化以及后续 View 失败时的事务语义。H-04D 下一步将单方向光 View 结果转换为 H-03 PBR
Render Graph Pass 输入。

## H-04D 实现记录

`scene_pbr_adapter` 将指定 `view_visibility` 转换为现有 `pbr_graph_pass_desc`。View 的
view-projection 和相机位置、唯一可见方向光，以及稳定排序后的可见 Renderable Model、Normal
Matrix 和 Object ID 均按 H-03 契约复制。颜色附件必需，深度附件保持可选。

适配器要求目标 View 至少有一个可见 Renderable 且恰好有一个可见方向光；零个或多个方向光均明确
失败，不能静默选择第一盏灯。它同时把排序后的原 Renderable 索引交给录制回调，使调用方能从快照
读取不透明 payload，并据此绑定 Mesh、Material 和录制 Draw。Scene 模块不解释 payload，也不持有
任何 GPU 资源。

`granit::scene` 现在按计划依赖 `granit::pbr`，依赖仍保持单向；PBR、Material、Render Graph 和核心
Renderer 均不包含 Scene 头文件。测试覆盖输入映射、可见顺序、附件声明、方向光数量、View 范围和
Render Graph 编译。H-04E 下一步补多 View 端到端生命周期测试与 100/1,000/10,000 对象性能基线。

## H-04E 实现记录

现有测试链已经覆盖多 View 共享场景所有权、输入容器修改、失败保留旧快照、逐 View Renderable 与
三类光源索引，以及 Scene 到 PBR Render Graph 编译闭环。快照销毁只释放自身值数组，不操作 payload
对应对象或任何 GPU 资源；真正进入录制回调后的资源生命周期继续由 Command Recorder 管理。

`granit_scene_benchmarks` 建立两个 View、100/1,000/10,000 对象的完整快照构建基线。Windows Clang
Release 下 P50 分别为 5.112 us、48.051 us 和 0.951 ms，P95 分别为 6.197 us、50.505 us 和
1.134 ms。当前近似线性增长，10,000 对象尚未稳定超过 1 ms，因此不引入空间树、内部任务系统、
SIMD 专用路径或 GPU culling。

H-05 必须沿用以下输入契约：

- 消费 `multi_view_snapshot` 的只读 View、可见对象和光源索引，不建立第二套 Scene 所有权。
- 方向光、点光和聚光的 Shader/Buffer 布局由 H-05 定义；H-04 的输入值保持后端无关。
- 阴影为独立 Render Graph Pass 和资源，结果通过 Group 3 或独立 Pass 输入进入 PBR，不污染材质
  Group 1。
- IBL、反射探针、天空和 Tone Mapping 属于 H-05，环境资源由 H-05/调用方持有，Scene 只提供必要
  的索引或空间输入。
- 聚簇/分块光照必须由真实多光源基准驱动；H-04 的 CPU 粗筛选仍作为小规模与回退路径。

H-04 首版至此完成。后续只有当目标场景的 Scene CPU 工作稳定超过 1 ms、对象规模明显超过 10,000，
或多 View 重复工作成为可测瓶颈时，才重新评估快照缓存、空间索引或外部执行器扩展点。
