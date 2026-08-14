<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-05：光照与后处理参考管线

## 状态

- 路线图任务：H-05
- 优先级：P2
- 状态：已完成
- 前置依赖：H-01 Render Graph、H-02 Material、H-03 PBR、H-04 Scene
- 后续依赖：H-07 参考 Render Pipeline
- 历史记录：[H-05 实施记录](../records/H-05-lighting-pipeline-implementation.md)

## 目标

H-05 在已验证的 PBR 与 Scene 输入之上建立可复用的参考光照和后处理模块：

- 支持有界的方向光、点光和聚光输入。
- 提供单方向光阴影参考路径。
- 提供 split-sum IBL 所需资源和绑定。
- 使用 HDR 中间目标与 ACES Tone Mapping 输出显示空间图像。
- 通过 Render Graph 组合完整离屏、窗口和多 View 路径。
- 建立功能降级、生命周期、像素和性能验证基线。

## 非目标

- 不建立 ECS、Scene Graph、资产数据库或光源组件系统。
- 不负责环境贴图卷积、prefilter mip 或 BRDF LUT 的离线生成工具。
- 不在当前阶段实现自动曝光、透明 PBR、CSM、点光阴影或聚光阴影。
- 不在缺少测量依据时引入 Tile/Cluster Light Culling。
- 不把 Lighting、PBR 或 Render Graph 反向放入核心 Renderer。

## 已确认渲染路径

当前参考路径是 Forward PBR，而不是 Deferred 或 Clustered Forward：

```text
Scene Snapshot / View
  -> Directional Shadow Pass
  -> Forward PBR HDR Pass
  -> ACES Tone Mapping
  -> LDR Output
```

- Shadow Pass 写入方向光深度图。
- PBR Pass 读取材质、可见光源、阴影和 IBL，写入 HDR Color 与 Depth。
- Tone Mapping Pass 读取 HDR Color，写入最终 LDR 目标。
- 多 View 共享场景输入，但每个 View 保有独立输出、尺寸和逐 View 资源。
- Render Graph 只理解 Pass 和资源依赖，不内置 PBR、阴影或 Tone Mapping 业务语义。

## 光源与质量边界

- 光源采用逐 View 可见列表和有界 GPU Buffer，不让光源数量无限增长。
- 方向光、点光和聚光使用固定 GPU 布局，并通过统一 Lighting Bind Group 提交。
- 首版阴影只支持一个主方向光；阴影分辨率和开关属于质量配置。
- IBL 使用 irradiance cubemap、prefiltered environment cubemap、BRDF LUT 和 Sampler。
- 缺失可选能力时按“阴影、IBL、附加光源”独立降级，不切换为另一套材质模型。
- 当前测量尚未证明需要 Clustered Forward；达到重新评估条件后再扩展。

## 绑定与所有权

H-05 沿用 H-02/H-03 的绑定约定：

- Material Group 保存材质常量和材质纹理。
- Frame/View/Object Group 保存对应频率的数据。
- Lighting Group 保存阴影、IBL、光源计数和光源 Buffer。

Lighting 可以拥有默认纹理、阴影图、中间 HDR 目标、Pipeline 和 Bind Group 缓存，但不拥有调用方
的 Scene、Mesh、Material、最终输出或资产数据库。所有外部资源均通过受校验句柄或只读逐帧描述
借用，并明确有效期。

## 完成结果

H-05 已完成：

- 多光源数据、打包和 CPU 数值参考。
- 单方向光阴影、split-sum IBL、HDR 与 ACES Tone Mapping。
- 功能降级组合和统一 Lighting Group。
- 离屏 PBR → HDR → Tone Mapping → LDR 像素闭环。
- Swapchain、Resize、双 View 和独立输出验证。
- 统一 Render Graph 组合与 2,000 帧生命周期压力测试。
- Windows/Linux、Clang/GCC/MSVC、共享/静态安装 Consumer 验证。
- CPU/GPU 基线；现有测量没有触发分块或聚簇光照。

逐阶段实现过程、测试命令和测量结论保存在
[H-05 实施记录](../records/H-05-lighting-pipeline-implementation.md)，性能原始结果位于
[`benchmarks/results`](../../benchmarks/results/README.md)。

## 重新评估条件

满足下列任一条件时，可以建立后续 Plan，但不直接改写 H-05 已完成边界：

- 多光源基准显示逐对象光源遍历或绑定成为主要瓶颈。
- 真实场景需要超过当前有界光源容量。
- 阴影质量需求明确要求 CSM、点光阴影或聚光阴影。
- 资产流程需要由 Granit 提供环境卷积和 LUT 离线工具。
- 自动曝光、透明 PBR 或高级后处理获得明确产品需求。

Clustered Forward、Bindless 和高级阴影必须分别记录数据依据、回退路径和像素验证方案。

## 验收标准

- 所有公共与高层接口不暴露 Vulkan 类型。
- 阴影、IBL、HDR 和 Tone Mapping 可以独立启停并获得确定结果。
- 离屏、窗口和多 View 使用同一资源与 Render Graph 模型。
- Renderer 销毁、资源提前失效和重复帧不会产生 Validation warning/error。
- 性能决策有可复现数据，不以经验推测替代测量。
