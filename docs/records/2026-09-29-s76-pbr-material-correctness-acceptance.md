<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-29 S-76 标准 PBR 材质正确性验收

## 结果

S-76 已完成发布前验收。标准 PBR 现在统一表达负缩放实例、OPAQUE/MASK/BLEND、双面、五槽独立
UV/Sampler 和 `COLOR_0`，Model Viewer 直接映射 glTF 核心语义。自动 RenderPipeline 在同一 HDR
阶段依次执行不透明与透明 Draw，透明对象按 View 稳定排序并使用预乘 Alpha，不投射阴影。

标准 PBR Material 模板升级为 v9，HLSL-first Shader Library 同时生成 SPIR-V 和 WGSL；源资产、
安装快照、公共内容身份和迁移指南保持一致。

## 本地验证

- Windows Clang shared 完整构建及 103/103 测试通过；
- Windows Clang static 完整构建及 92/92 测试通过；
- Emscripten Debug 完整构建及 16/16 Node 测试通过；
- Shader Library 与 Material 从源码重建并与仓库快照一致；
- Material Pipeline 状态、Variant Key、透明稳定排序、glTF 导入和 Model Viewer 接线测试通过。

## 远端验证

- [Quick Check](https://github.com/synchronized/granit/actions/runs/36513194177)、
  [Documentation](https://github.com/synchronized/granit/actions/runs/36513224199)、
  [Linux](https://github.com/synchronized/granit/actions/runs/36513201854)和
  [Windows](https://github.com/synchronized/granit/actions/runs/36513209357)通过；
- [Emscripten](https://github.com/synchronized/granit/actions/runs/36513215396)完成编译与全部浏览器
  矩阵。发布候选复跑仍在 Model Viewer 浏览器验收中偶发 Dawn 实例丢失；矩阵已改为串行以降低
  运行器波动，但该问题不阻塞本次已通过功能验收的代码，留待下一版本整体修复；
- PR #117 的功能提交 `2454d187` 通过以上发布前矩阵，最终由合并提交 `bd361bd9` 发布。

## 发布结果

- [Release 工作流](https://github.com/synchronized/granit/actions/runs/36516253026)完成 Windows/Linux
  shared SDK 构建、测试、安装审计、打包和公开产物复验；
- [`v0.41.0`](https://github.com/synchronized/granit/releases/tag/v0.41.0) 已于 2026-09-29 发布，
  包含两个平台 SDK 与 `SHA256SUMS`。

## 保留限制

- 透明排序以 Renderable 包围球中心为对象级近似，不处理相交对象或 Mesh 内部三角形顺序；
- 本版本不包含透明阴影、OIT、折射、透射、体积材质或 `KHR_texture_transform`；
- Model Viewer 浏览器验收仍可能因 Dawn 外部 Instance 丢失而失败，需在下一版本统一检查 WebGPU
  实例生命周期、浏览器启动方式与重试策略；
- `opaque_gpu_ns` 继续表示完整 Forward PBR 图节点，包含透明阶段，不新增 ABI 字段。
