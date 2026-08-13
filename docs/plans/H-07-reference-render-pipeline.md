<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-07：高级参考渲染套件

## 状态

- 路线图任务：H-07
- 优先级：P2
- 状态：进行中，公共 Mesh 和 Mesh Draw 录制已完成，正在收敛内置 PBR 绑定
- 必需依赖：H-01 Render Graph、H-02 Material、H-03 PBR、H-04 Scene、H-05 Lighting/Post Process
- 可选依赖：H-06 2D/UI/调试绘制评估结果

## 定位

H-07 将已经验证的高层模块组合为类似 DiligentFX 的可选参考渲染套件。它提供一条默认可运行的
实时 PBR 帧管线，但不成为 Granit Renderer 的组成部分，也不阻止使用者自行组织全部渲染流程。

这套能力吸收 DiligentFX 的模块组合与工程分层经验，并参考 Filament 的 PBR、光照、色彩管理和
最终画面质量；不要求兼容两者的 API、对象模型、Shader、材质包或资产格式。

## 渲染路径

H-07 当前采用 Forward PBR，而不是 Deferred Rendering：Shadow 之后，每个可见对象在 PBR HDR
Pass 中直接完成材质与光照计算，再进入 Tone Mapping。当前没有 G-Buffer 或独立 Deferred
Lighting Pass。

当前也尚未达到 Forward+/Clustered Forward：逐 View 光源筛选与批量打包已经存在，但没有
Tile/Cluster 划分和 Clustered Light Culling。后续优先沿现有路径增加 Clustered Forward；Deferred
保留为可选高级管线，不替换参考管线，也不进入核心 Renderer 职责。详细边界见
[架构文档](../architecture.md#渲染路径定位)。

## 三种使用模式

### 只使用 Renderer

面向自研引擎、特殊渲染器和高级用户。使用者直接管理 Buffer、Texture、Pipeline、Bind Group、
Command Recorder、同步和提交，不构建任何 Granit 高层目标。

### 选择部分高层模块

使用者按需组合 `granit::render_graph`、`granit::material`、`granit::scene`、`granit::pbr`、
`granit::lighting` 或 `granit::post_process`，并自行决定 Pass 顺序与资源所有权。

### 使用完整参考管线

使用者创建独立的 `granit::render_pipeline` 对象，提交 Scene 快照、输出目标和显式环境输入，由它
组合阴影、PBR、环境光照和后处理。统一门面是便利层，不隐藏底层资源句柄，也不成为唯一入口。

## 依赖与发布边界

```text
应用或游戏引擎
├─ 直接使用 granit::granit
├─ 选择高层模块
└─ granit::render_pipeline
   ├─ granit::scene
   ├─ granit::material / granit::pbr
   ├─ granit::lighting
   ├─ granit::post_process
   └─ granit::render_graph

所有高层模块 -> granit::granit -> Vulkan 后端
```

- `granit::granit` 不得包含、链接或了解任何高层模块。
- 每个高层模块及统一门面必须能够独立选择是否构建。
- 高层目标不能成为核心安装包的传递依赖；后期可作为单独 CMake component 安装导出。
- 高级层以 C ABI 作为动态库边界，并在其上提供轻量 C++20 包装；当前开发阶段暂不承诺 ABI 稳定。
- 高级层只能使用 Granit 公共 Renderer API，不得绕过封装直接调用 Vulkan。

公共 ABI 收敛检查点确认后，上述发布边界进一步固定为：高层功能由独立
`granit_render_pipeline` 动态库或静态库导出，使用单独的 `GRANIT_RENDER_PIPELINE_API` 符号宏；
它依赖 `granit::granit`，但核心库不反向链接或安装高层头文件。使用者可以只安装核心 component。

## 所有权

`render_pipeline` 可以拥有自身创建的 Pipeline、Bind Group、默认纹理、阴影图、中间 HDR 目标和
缓存，但不拥有调用方的以下对象：

- ECS、Entity、Scene Graph 或 Transform 层级。
- `multi_view_snapshot` 来源数据和 Renderable payload 对应对象。
- Mesh、材质实例和资产数据库。
- 调用方导入的最终输出、环境贴图和外部 Render Graph 资源。

每帧提交使用只读快照和值描述。需要跨帧保存的外部对象只记录受校验的 Granit 句柄，并明确有效
期；不能长期保存调用方容器地址。

## 建议门面

以下只是 C++20 方向草案，不构成当前 API：

```cpp
granit::render_pipeline pipeline;
pipeline.initialize(renderer, options);

pipeline.render({
    .scene = snapshot,
    .view_index = 0,
    .target = output,
    .environment = environment,
});
```

`render` 不应接收或拥有通用 ECS。首版输入保持显式，并允许调用方提供录制 Mesh/Material Draw 的
回调。只有当 H-05 完整管线证明重复接线稳定后，才确定最终门面字段。

## 可扩展位置

完整参考管线至少为以下位置保留显式替换或插入能力：

- Shadow caster 录制。
- Opaque PBR Draw 录制。
- 天空或背景。
- Tone Mapping 前 HDR Pass。
- Tone Mapping 后显示空间 Pass。
- 最终输出或 UI 合成。

扩展采用 Granit 资源、Render Graph ID 和回调，不暴露 Vulkan Command Buffer、Image Layout 或
Pipeline Stage。首版不设计任意插件系统；只为已经存在的固定阶段提供受控扩展点。

## 与 Filament 的边界

参考 Filament：

- 金属度/粗糙度 PBR 数学与能量关系。
- IBL、阴影、曝光、Tone Mapping 和颜色输出的一致流程。
- Shader 变体裁剪、移动 GPU 性能和数值稳定性经验。

不复制 Filament：

- 不让核心 Renderer 拥有 Engine、Scene、View、Camera 或 Light。
- 不将 `render(view)` 设为底层 Renderer 的唯一工作方式。
- 不要求采用 Filament Material、资产格式或完整 FrameGraph 架构。

## 与 Diligent/DiligentFX 的边界

参考 Diligent：

- Texture/View、Pipeline、资源布局、资源状态和命令职责分离。
- 底层渲染接口与 DiligentFX 风格高层组件之间的单向依赖。
- 高层模块可以绕过、替换和独立发布。

不复制 Diligent：

- 不采用 COM 风格虚接口、引用计数对象和跨动态库 C++ 对象 ABI。
- 不为尚未计划的多图形后端建立最低公共能力集。
- 不兼容 Shader Resource Binding 或 DiligentFX 内部资源格式。

## 实施条件

H-07 只有在以下条件满足后进入实现：

- H-05 完成阴影、IBL、HDR/Tone Mapping 的离屏闭环。
- 各高层模块的资源所有权、错误语义和生命周期测试稳定。
- 至少一个示例已经重复出现可由统一门面消除的接线代码。
- 完整组合不会迫使核心 `granit::granit` 增加 Scene/PBR 专用接口。

H-06 的 UI/调试绘制结果可以作为最终合成扩展，但不作为 H-07 首版的硬依赖。

H-05 完成后先执行公共 API 收敛检查点：确定 Scene、Material、Lighting 和统一渲染入口的首版 C
ABI、描述结构、整数句柄、所有权与线程语义，并在其上提供轻量 C++20 RAII 包装。H-07 使用该检查
点确认的粗粒度接口实现门面，不把当前内部类、Group 3 binding 或 Render Graph 实现类型直接导出。

## 公共 ABI 收敛检查点

首版高层 ABI 只发布三个 64 位整数句柄，零值无效：

- `granit_material`：拥有不可变模板版本和可更新实例参数；Shader 变体与 Pipeline 缓存在库内。
- `granit_scene_snapshot`：拥有一次事务式复制后的 Renderable、光源和 View 值数据，不拥有 ECS、
  Mesh、Material 或 payload 指向的外部对象。
- `granit_render_pipeline`：拥有默认资源、阴影/HDR 中间目标、Pass 组合和跨帧缓存，借用 Renderer。

不为 Camera、Light、View、颜色、矩阵或单个 Renderable 创建句柄。它们使用包含 `struct_size` 的 C
值结构；数组统一使用“只读指针 + `uint32_t` 数量”，成功返回前完成复制。向量和矩阵复用
`granit/math/types.h` 的 ABI 值类型；矩阵明确为 16 个 `float` 的列主序布局。该目录不代表完整
数学库，内部计算函数仍不属于公共 ABI。

### 粗粒度调用顺序

```text
创建 Renderer
  -> 创建/更新 Material
  -> 从借用数组创建 Scene Snapshot
  -> 创建 Render Pipeline
  -> render(pipeline, snapshot, view range, output, environment)
  -> 销毁 Pipeline / Snapshot / Material
  -> 销毁 Renderer
```

`render` 一次处理一个或多个 View，并在库内完成可见性、光源打包、Shadow、PBR HDR、Tone Mapping
和批量提交。ABI 不公开逐 Pass 函数，不为每个对象跨 DLL 调用一次函数。Renderable 使用稳定的
`uint64_t payload` 通过单次渲染的只读关联表映射到 `granit_mesh` 和 `granit_material`。管线完成
可见性后按可见顺序生成批次，并调用录制回调；回调参数包含核心 `granit_command_recorder`、只读
Renderable、payload 和关联项数组。回调不得保存数组地址，不得结束、提交或销毁 Recorder，也不得
递归调用同一个 Pipeline。

### 所有权、线程与失败语义

- 三类高层句柄均校验 generation、类型及所属 Renderer；销毁后旧句柄立即失效。
- Material 更新和 Snapshot 创建采用事务语义；失败时旧状态保持不变。
- Pipeline、Material 和 Snapshot 的同一句柄操作串行化；不同句柄可并行。单个 Pipeline 的
  `render` 首版不可并发，但可以在外部线程调用。
- Pipeline 强引用录制和在途提交所需的内部资源；外部输出、环境 Texture View 和 Renderer 只借用，
  必须覆盖调用期间及 GPU 完成期，或由核心 Recorder 的既有保活机制接管。
- 回调返回 `granit_result`；首个错误终止本帧且不提交未完成 Recorder。异常不得跨越 C ABI。
- C++20 包装仅提供 move-only RAII、`std::span` 和强类型枚举，不保存第二套 Scene、Material 或
  Pipeline 状态。

### 明确不进入首版 ABI

- 内部 Render Graph ID、Pass ID、Group 编号、binding、Shader 变体键和 Vulkan 类型。
- ECS、Scene Graph、Camera 层级、资产加载、Mesh 所有权和通用插件系统。
- 每个 Draw 一次的公开动态库调用、任意 Command Buffer 注入或跨 ABI C++ 回调对象。
- ABI 稳定承诺；项目仍处于开发状态，首版实现验证期间允许调整尾部字段和函数集合。

### 当前实现进度

公共 ABI 的第一块已经落地为独立 `granit::render_pipeline` 构建目标：

- `granit/pipeline/scene.h` 提供 C11 可包含的 Scene Snapshot 值描述、创建和销毁接口。
- `granit/pipeline/scene.hpp` 提供不保存平行状态的 move-only C++20 RAII 包装。
- 创建时复制全部 View、Renderable 与光源值数据，并在验证完成后一次性发布新句柄。
- Snapshot 句柄校验类型、generation 和所属 Renderer；失败创建统一清空输出句柄。
- 生命周期测试覆盖成功创建、重复销毁、槽位复用后的旧句柄、跨 Renderer 和非法数组。
- `granit/pipeline/material.h` 从材质归档创建 `granit_material`，并以一批参数作为事务更新单位。
- Material 更新先迁移到候选 GPU 实例，参数校验、Buffer 和 Bind Group 刷新全部成功后才替换原实例；
  失败不会留下 CPU 参数与 GPU 绑定不一致的半完成状态。
- Material C++20 包装命名为 `granit::material_instance`，避免与现有内部 `granit::material`
  模块命名空间冲突；包装只保存 Renderer 和 C 句柄。
- Material 句柄同样校验类型、generation 和所属 Renderer；不同材质只在短暂句柄查询时共享锁，
  GPU 更新由各实例独立串行化。
- `granit/pipeline/render_pipeline.h` 已提供统一 Pipeline 句柄与单次多 View `render` 入口。首版实现
  从 Scene Snapshot 复制稳定输入，逐 View 执行可见性结果，并组合 PBR HDR、Depth 与 Tone Mapping。
- 固定阶段回调目前覆盖 Shadow 与 Opaque。回调获得 Recorder、HDR/Depth 输出 View、当前 View、可见
  Renderable，以及已按 payload 关联的 Mesh/Material 批次；不得提交 Recorder 或递归调用同一
  Pipeline。重复、缺失、无效或跨 Renderer 的 Mesh/Material 均在录制前失败。
- 回调失败时本 View 不提交未完成 Recorder；同一 Pipeline 并发或递归调用返回 `NOT_READY`。
- Tone Mapping Shader 作为构建输入嵌入 `granit_render_pipeline`，运行时不读取示例或测试资产。
  Granit 在固定 Pass 内创建绑定、录制全屏 Draw，并根据 UNORM/sRGB 输出选择唯一颜色编码路径。
- Tone Mapping 的 Shader、Sampler、Bind Group Layout、Pipeline Layout 和 Graphics Pipeline 已按输出
  编码缓存并跨 View、跨帧复用；依赖瞬态 HDR View 的常量 Buffer 与 Bind Group 仍按 View 创建。
- 首个可见方向光已生成 1024×1024 单级正交阴影 Pass。Shadow 回调获得投影者批次与光源
  View-Projection，Opaque 回调获得只在回调期间有效的阴影 Texture View；级联阴影和阴影图缓存
  尚未实现。
- Pipeline 创建时生成并缓存默认黑色 Irradiance/Prefiltered Environment Cubemap 与中性 BRDF LUT，
  Opaque 回调获得完整 IBL View、Layout 和 Bind Group。默认环境强度为零，不会意外改变直接光结果；
  外部环境资产和运行时切换接口仍属于后续增量。
- 公共 `granit_render_pipeline_*` C ABI 已覆盖真实离屏像素回归：通过阶段回调清屏 HDR Attachment，
  由内置 Tone Mapping 输出 RGBA8，再使用公共 Texture-to-Buffer Readback 校验中心像素。该测试不再
  只以回调次数或内部 Graph 状态作为成功依据。

`RenderPipeline` CMake component 已安装导出公开头文件和 `granit::render_pipeline`。共享构建安装
独立动态库；静态构建同时导出不带公共头文件的 `granit::detail_*` 依赖闭包，使用者不应直接链接
这些实现目标。安装后的纯 C 与 C++20 consumer 已在共享和静态配置下完成编译、链接与运行验证。
外部环境资产接口、实际 Mesh Draw 门面、级联阴影和 Clustered Forward 仍属于后续增量。

## 分步实施

### H-07A：组合边界

- **已完成**：盘点 H-05 示例中的重复资源、Pass 和错误处理。
- **已完成**：确定 `render_pipeline` 创建、并发、销毁和错误语义。
- **已完成**：明确外部目标、默认环境、Mesh/Material 关联和多 View 输入。

### H-07B：单 View 门面

- **已完成**：组合方向光阴影、Forward PBR、默认 IBL 和 Tone Mapping。
- **已完成**：使用统一 Texture View 输出模型，公共 ABI 离屏像素回归通过。
- **已完成**：保留 Shadow 与 Opaque 受控录制回调；内置 Mesh Draw 尚待 H-07F。

### H-07C：多 View 与缓存

- **已完成**：消费 H-04 多 View 快照，并按 View 生成稳定可见批次。
- **已完成**：跨帧复用 Tone Mapping Pipeline 与默认 IBL 资源。
- **已确认**：暂不跨 View 共享阴影或中间资源，等待真实帧测量。

### H-07D：发布与示例

- **进行中**：直接 Renderer 与部分模块示例已存在；完整自动 Draw 参考管线示例等待 H-07G。
- **已完成**：独立 `RenderPipeline` component，以及共享/静态 C 与 C++20 consumer。
- **已完成**：记录 Forward PBR 能力范围和可绕过边界。

### H-07E～H：可用门面收尾

1. **H-07E 公共 Mesh ABI（已完成）**：建立受 Renderer domain 校验的 `granit_mesh`，显式描述
   Vertex/Index Buffer、顶点布局、索引类型和 Draw Range，不接管资产或 CPU 数据。一个 Mesh
   对应一次不可变 Draw，不含 Submesh 数组。Mesh 借用 Buffer；独立动态库不穿透核心库持有内部
   引用，创建时查询用途与范围，后续录制时再次校验，调用者必须保证 Buffer 生命周期覆盖 Mesh。
2. **H-07F 内置 Draw**：让 Pipeline 使用 Mesh 与 Material 自动录制 Opaque/Shadow Draw，回调改为
   可选高级扩展点，普通用户不再必须理解 Command Recorder。Mesh 内部已能
   在录制时重新校验借用 Buffer，并绑定 Vertex/Index Buffer 后发出对应 Draw；剩余工作是
   Group 0/2 帧与对象 Uniform Layout，以及 Group 3 Shadow/IBL/Light Superset Layout 已纳入
   公共 Material 的 Pipeline Layout；剩余工作是创建对应 Buffer/Bind Group，并由 Shadow/Opaque
   阶段绑定 Group 0～3 后调用 Mesh 录制器。

   Group 3 资源可借用 Material 持有的外部 Layout 创建 Bind Group，并且不在重置时销毁
   该 Layout。这保证 Pipeline Layout 与 Bind Group 使用同一布局对象，而不只是字段相同。
   Render Pipeline 可通过不导出的 Material Access 一次获取指定 Pass/变体/附件格式
   对应的 Graphics Pipeline、Pipeline Layout、Group 0～3 Layout 和 Group 1 Material Bind
   Group。这些只是 Material 存活期内的内部借用句柄，不进入 C ABI。
   内部 `pbr_draw_bindings` 已能将 H-03 的 112 字节 Frame 常量与 144 字节 Object
   常量上传为 Uniform Buffer，并使用 Material 的原始 Layout 创建 Group 0/2 Bind Group。
   首版按 Draw 持有这两组资源；只在测量证明创建成本显著后再增加环形 Buffer 或缓存。
3. **H-07G 完整示例**：提供安装 API 下的离屏和窗口完整路径，并保留直接 Renderer 用法作为对照。
4. **H-07H 验收**：补齐生命周期压力、Resize、多 View、Validation Layer、输出一致性和性能对比。

外部环境切换、透明物体、CSM 和 Clustered Forward 在 H-07H 后单独立项，不阻塞首版完成。

## 验收标准

- 不链接 H-07 时，核心库的接口、二进制依赖和构建结果不变。
- 使用者可以完全绕过门面，复用同一 Renderer 资源自行录制命令。
- 完整门面不拥有 ECS、Scene Graph、Mesh、Material 或外部目标。
- 离屏、窗口和多 View 使用一致的资源与 Pass 模型。
- 关键阶段可以通过受控回调替换或插入，不需要 Vulkan 互操作。
- 与手工组合 H-05 模块相比，统一门面没有不可解释的输出差异或显著性能退化。
