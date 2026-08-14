<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-07：高级参考渲染套件

## 状态

- 路线图任务：H-07
- 优先级：P2
- 状态：进行中；H-07A～H-07G 已完成，H-07H 收尾验收进行中
- 必需依赖：H-01～H-05
- 可选后续：H-06 Unlit、2D 与 UI
- 历史记录：[H-07 实施记录](../records/H-07-reference-render-pipeline-implementation.md)

## 定位

H-07 将已经验证的 Render Graph、Material、PBR、Scene 和 Lighting 模块组合成可选的高级参考
Render Pipeline。它提供默认可运行的 Forward PBR 帧路径，但不成为核心 Renderer 的组成部分，
也不阻止使用者自行组织渲染流程。

```text
Scene Snapshot
  -> Directional Shadow
  -> Forward PBR HDR
  -> ACES Tone Mapping
  -> LDR Output
```

当前没有 G-Buffer 或 Deferred Lighting Pass，也没有 Tile/Cluster Light Culling，因此不称为
Deferred 或 Forward+。详细渲染路径见[架构说明](../concepts/architecture.md#渲染路径定位)。

## 四种使用层级

1. **完整参考管线**：提交 Scene、View、输出和质量配置，由 Pipeline 组织完整帧。
2. **扩展参考管线**：配置 Material，并在稳定位置插入后处理、调试或 UI Pass。
3. **自建 Render Graph**：选择部分高层模块，自行决定 Pass、资源依赖和所有权。
4. **直接使用 Renderer**：自行管理 GPU 资源、命令、同步和提交。

PBR 决定着色模型，Render Pipeline 决定整帧策略，Render Graph 组织 Pass 与资源依赖，Renderer
负责实际 GPU 命令。任何一层都不复制相邻层的运行时状态。

## 依赖与发布边界

```text
应用或游戏引擎
├─ granit::granit
├─ 可选高层模块
└─ granit::render_pipeline
   ├─ Scene / Material / PBR
   ├─ Lighting / Post Process
   └─ Render Graph

所有高层模块 -> granit::granit -> Vulkan 后端
```

- 核心 `granit::granit` 不得包含、链接或了解高层模块。
- `RenderPipeline` 作为独立 CMake component 安装，可构建为共享库或静态库。
- 高层动态库以 C ABI 为边界，并提供不保存平行状态的 C++20 RAII 包装。
- 高层实现只使用 Granit 公共 Renderer API，不直接调用 Vulkan。
- 当前仍处于开发阶段，不承诺 API 或 ABI 稳定。

## 公共对象与所有权

首版高层 ABI 使用以下受校验 64 位句柄：

- `granit_mesh`：描述一次不可变 Draw，借用 Vertex/Index Buffer，不拥有资产或 CPU 数据。
- `granit_material`：拥有不可变模板版本和可更新 GPU 实例。
- `granit_scene_snapshot`：拥有事务式复制后的 View、Renderable 和光源值数据。
- `granit_render_pipeline`：拥有默认资源、中间目标、Pass 组合和跨帧缓存，借用 Renderer。

Camera、Light、View、矩阵和单个 Renderable 使用包含 `struct_size` 的 C 值结构，不创建细粒度
句柄。Pipeline 不拥有 ECS、Scene Graph、Mesh、Material、资产数据库、最终输出或外部环境资源。

每帧输入在成功返回前完成必要复制。Recorder 保活已录制和在途提交使用的 Granit 资源；外部对象
仍须满足各自公开生命周期约束。

## 渲染与扩展边界

一次 `render` 处理一个或多个 View，在库内完成可见性、光源打包、Shadow、PBR HDR、Tone Mapping
和批量提交。多 View 必须提供等长输出数组，每个 View 使用独立目标、格式和尺寸。

默认路径自动录制 Mesh 的 Shadow 与 Opaque Draw。高级用户可以覆盖固定阶段回调，但回调：

- 只接收 Granit Recorder、资源句柄和只读批次。
- 不得保存临时数组地址。
- 不得提交、结束或销毁 Recorder。
- 不得递归调用同一 Pipeline。
- 返回首个错误时终止本帧，并且不提交未完成 Recorder。

后续扩展位置包括天空背景、Tone Mapping 前 HDR Pass 和 Tone Mapping 后 UI Pass。首版不提供任意
插件系统，也不暴露 Vulkan Command Buffer、Image Layout 或 Pipeline Stage。

## 已完成范围

H-07A～H-07G 已完成：

- 独立 `granit::render_pipeline` 目标、C ABI、C++20 包装和安装 component。
- 公共 Scene Snapshot、Material、Mesh 和多 View Render Pipeline 接口。
- 方向光阴影、自动 Opaque/Shadow Draw、默认 IBL 和 Tone Mapping 缓存。
- 内置 Shadow/Tone Mapping Shader，不在运行时读取示例或测试资产。
- 公共 API 离屏像素回归，以及 Swapchain、Frame 和 Resize 窗口路径。
- 共享/静态构建下的纯 C 与 C++20 安装 Consumer。
- 多 View 独立输出、资源提前失效和 Validation 诊断测试。

逐阶段接口演化、内部绑定和验证细节见
[H-07 实施记录](../records/H-07-reference-render-pipeline-implementation.md)。

## 剩余任务

### H-07H：收尾验收

- 补齐自动路径与手工 H-05 组合的输出一致性比较。
- 补齐统一门面的 CPU/GPU 性能对比，确认没有不可解释的显著退化。
- 汇总离屏、窗口、多 View、Resize、Validation、生命周期和安装 Consumer 结果。
- 验收通过后将 H-07 标记为完成，并把长期行为同步到对应 Reference。

外部环境切换、透明 PBR、CSM 和 Clustered Forward 后续单独立项，不阻塞 H-07 首版完成。

## 验收标准

- 不链接 H-07 时，核心库的接口、二进制依赖和构建结果不变。
- 使用者可以绕过参考管线，复用同一 Renderer 资源自行录制命令。
- Pipeline 不拥有 ECS、Scene Graph、Mesh、Material 或外部目标。
- 离屏、窗口和多 View 使用一致的资源与 Pass 模型。
- 关键阶段可以通过受控回调替换，不要求 Vulkan 互操作。
- 相对手工 H-05 组合没有不可解释的输出差异或显著性能退化。
