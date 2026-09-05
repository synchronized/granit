<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-05 S-22C/S-22D SDK 边界审计

## 结论

Granit 的公共头未泄漏 Vulkan、WebGPU、Dawn、平台宏或 `src/` 内部头。Gneiss 当前只使用已安装
导出的 Core、Window、Input 与 RenderPipeline 目标和公共头，没有形成需要提升上游类型到 Granit
SDK 的新需求。

审计发现一项 Granit 安装包缺陷：0.x 包曾采用 `SameMajorVersion`，会让请求 0.1 的 Consumer
接受 0.6 SDK，与“次版本允许破坏性变更”的项目策略冲突。S-22 已改为同一次版本兼容，并加入
旧次版本拒绝测试。

## 审计范围

- Granit `include/granit` 中的 C11、C++20、Shader 资产和可选 component 入口。
- Granit 安装后的 Core-only、RenderPipeline、Window、Input 与 ShaderTools CMake 目标。
- Gneiss 的 Granit Provider、平台适配、渲染服务、资源关闭和安装运行时逻辑；检查过程只读，
  未修改 Gneiss。

## 结果

- Core-only 选包不会隐式导入其他 Granit component。
- Input 会显式导入其 Window 依赖；RenderPipeline 和 Window 可独立请求。
- ShaderTools 仅在 SDK 实际安装该 component 时可请求，缺失时配置阶段明确失败。
- Gneiss 可通过 `find_package` 或 FetchContent 取得相同四个公共目标，不包含 Granit 内部头。
- Gneiss 自行拥有应用循环、资源镜像、场景快照、Shader 和渲染服务，符合 Granit 不接管上游
  场景与任务系统的边界。
- Gneiss 使用 Core 的底层 Pipeline 路径并按需使用 RenderPipeline 的 Canvas/Debug Draw，说明
  Core 与高层 component 保持可组合关系是必要的，不应强制统一到一套高层入口。
- 当前没有证据支持公开示例 glTF 加载器、Model Viewer 渲染线程或上游专用资源类型。

## 已执行验证

- Windows Clang Debug 共享与静态构建分别通过 75 项和 65 项测试。
- 从实际安装目录分别请求 Core、RenderPipeline、Window、Input 和 ShaderTools 通过。
- 对 0.1、0.4、0.5 的旧次版本请求均按预期拒绝，0.6 与 0.6.0 精确请求通过。
- 共享与静态安装后的 C/C++ Consumer 各 7 项通过。
- 文档检查与 `git diff --check` 通过。

跨平台共享/静态矩阵和浏览器 WebGPU 留到 S-22E 发布候选验收执行。
