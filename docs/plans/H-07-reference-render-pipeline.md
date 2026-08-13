<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-07：高级参考渲染套件

## 状态

- 路线图任务：H-07
- 优先级：P2
- 状态：已确认，等待公共 ABI 收敛检查点完成
- 必需依赖：H-01 Render Graph、H-02 Material、H-03 PBR、H-04 Scene、H-05 Lighting/Post Process
- 可选依赖：H-06 2D/UI/调试绘制评估结果

## 定位

H-07 将已经验证的高层模块组合为类似 DiligentFX 的可选参考渲染套件。它提供一条默认可运行的
实时 PBR 帧管线，但不成为 Granit Renderer 的组成部分，也不阻止使用者自行组织全部渲染流程。

这套能力吸收 DiligentFX 的模块组合与工程分层经验，并参考 Filament 的 PBR、光照、色彩管理和
最终画面质量；不要求兼容两者的 API、对象模型、Shader、材质包或资产格式。

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
- 高级层首先使用 C++20 接口，在行为成熟前不扩展稳定 C ABI。
- 高级层只能使用 Granit 公共 Renderer API，不得绕过封装直接调用 Vulkan。

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

## 分步实施

### H-07A：组合边界

- 盘点 H-05 示例中的重复资源、Pass 和错误处理。
- 确定 `render_pipeline` 创建、重建、Resize 和销毁语义。
- 明确外部目标、环境资源、Mesh/Material 回调和多 View 输入。

### H-07B：单 View 门面

- 组合方向光阴影、前向 PBR、IBL 和 Tone Mapping。
- 支持离屏与 Swapchain Backbuffer 的统一输出。
- 保留明确的 Pass 插入和录制回调。

### H-07C：多 View 与缓存

- 消费 H-04 多 View 快照，避免复制共享场景数据。
- 复用兼容的 Pipeline、默认资源和环境输入。
- 只有测量证明收益后才共享阴影或中间资源。

### H-07D：发布与示例

- 提供直接 Renderer、部分模块、完整参考管线三个并列示例。
- 增加独立构建、安装 component、consumer 和生命周期回归。
- 记录能力范围，避免把参考管线宣传为必须采用的引擎框架。

## 验收标准

- 不链接 H-07 时，核心库的接口、二进制依赖和构建结果不变。
- 使用者可以完全绕过门面，复用同一 Renderer 资源自行录制命令。
- 完整门面不拥有 ECS、Scene Graph、Mesh、Material 或外部目标。
- 离屏、窗口和多 View 使用一致的资源与 Pass 模型。
- 关键阶段可以通过受控回调替换或插入，不需要 Vulkan 互操作。
- 与手工组合 H-05 模块相比，统一门面没有不可解释的输出差异或显著性能退化。
