<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-006：以 Shader Library 作为运行时 Shader 资产

- 状态：已接受
- 日期：2026-09-10

## 背景

0.20.0 的 `.grshader` 使用后端无关清单和 WGSL/SPIR-V sidecar，但 Material resolver 仍接收 Renderer
后端和能力档位。应用资产层因此需要选择具体 sidecar，Material、示例和 RenderPipeline 也出现了
后端判断。单 Shader 文件适合离线增量编译，却不是理想的游戏运行时交付单元。

## 决策

- 0.21.0 引入 `.grshlib` Shader Library，集中保存 Shader 记录、反射契约、变体索引和去重载荷。
- `.grmat` 只通过稳定内容 ID 引用 Shader；Renderer 根据自身能力在 Library 内选择载荷。
- 发布资产按 Vulkan、WebGPU 或全后端目标裁剪，同一运行时接口不因目标改变。
- `.grshader` 不再作为安装 SDK 或应用部署资产；单 Shader 中间结果改名为 `.grshaderobj`，作为
  工具私有缓存且不承诺格式兼容。
- Material 创建改为借用显式 Shader Library 句柄，删除带 backend/profile 的 Shader resolver。
- Shader Library 不接管文件 I/O、网络、异步调度和资产数据库；调用方继续提供内存字节并管理
  其有效期。

## 影响

- 应用只需加载 `.grmat` 和一个或多个 `.grshlib`，不再识别 SPIR-V、WGSL 或 Renderer 后端。
- Material、Shader Library 和 Renderer 形成明确父子生命周期；Library 被 Material 使用时不能销毁。
- 离线工具增加确定性链接、重复 ID 检查、载荷去重和目标裁剪步骤。
- 这是 0.x 阶段的破坏性资产与 ABI 变更，需要更新 `.grmat`、公共头、ABI 快照和迁移指南。

## 替代方案

- 继续由 resolver 按 backend/profile 返回 sidecar：实现简单，但后端选择持续泄漏给应用。
- 把所有 Shader 直接嵌入每个 `.grmat`：运行时简单，但跨材质重复载荷，无法形成共享 Shader 缓存。
- 只提供全后端胖包：接口简单，但 Web 或 Vulkan 单目标发布无法裁剪无用载荷。
