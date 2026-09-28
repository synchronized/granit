<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-29 S-75 Deferred Rendering 本地验收

## 结果

S-75A～E 已完成本地验收。`07_deferred` 使用现有公共 API 在 Vulkan 与浏览器 WebGPU 运行同一套
MRT G-buffer、全屏 Lighting、调试视图和 Resize 资源重建流程。实现没有发现必须由新公共 API
解决的缺口，因此本版本没有增加 Transient Texture 或 Render Graph 接口。

浏览器首轮验证发现 Lighting Shader 的资源被编译到 descriptor set 1，而 Pipeline Layout 只包含
set 0。WebGPU Validation 将 Pipeline 判为无效；修正 HLSL binding 后，两个后端统一使用 set 0，
浏览器验证不再产生错误。

## 交付

- 新增 `examples/tutorials/07_deferred`，包含三颜色附件、Depth、实例场景和最多 64 个点光源；
- Final Lighting、Position/Depth、Normal、Albedo 四种模式复用同一个 Lighting Pipeline；
- 初始化查询颜色附件数量、Uniform 大小和 Texture 格式 Usage，不支持时明确返回 `unsupported`；
- G-buffer Bind Group、View 和 Texture 按依赖顺序重建，Desktop/Web 共用主体代码；
- 浏览器导出 Pass 数、光源数和输出模式，并自动切换四种模式后验证 Resize；
- 教程索引、能力矩阵、Emscripten 产物审计和浏览器 CI 矩阵已同步更新。

## 本地验证

- Windows Clang shared 完整构建及 103/103 测试通过；
- Windows Clang static 完整构建及 92/92 测试通过；
- `granit.tutorial.07_deferred` Vulkan Smoke 通过；
- Emscripten Debug 目标编译通过；
- 本机 Chrome WebGPU 多帧、三 Pass 状态、四种输出模式、像素差异和 Resize 验证通过；
- HLSL-first Shader Library 从源码重建并与仓库快照一致。

Linux、MSVC、Release Emscripten、安装 SDK 和共享发布包由 Pull Request 与 Release 工作流继续验证。
