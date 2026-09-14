<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-14 S-39/S-40 本地发布验收

## 结果

0.23.0 在分支 `feat/0.23-empty-frame-reliability` 上完成 Granit 本地发布验收。空场景仍会完成
clear-only Opaque、Tone Mapping、Canvas、Overlay 和 Frame 提交；Shader Toolchain 配置只在统一
根目录、DXC 与 Tint 同时有效时生成资产，旧分离缓存会安全回退到锁定快照。

本次没有增加或删除公共符号。Core、RenderPipeline、Window、Input 与 AssetTools 继续使用现有
ABI 快照回归。

## 本地验证

- Windows VS2022 共享 Debug：完整构建与 106/106 测试通过，包括 ABI 导出、Vulkan Smoke、
  空场景离屏像素和窗口 Swapchain Frame。
- Windows VS2022 静态 Debug：`GRANIT_SHADER_TOOLCHAIN_MODE=off` 下完整构建与 85/85 测试通过。
- 共享与静态安装结果均通过导出及资产审计、分组件选包和版本选择检查；两套独立 C11/C++20
  Consumer 各通过 7/7 测试。
- Emscripten Release 增量构建通过；0.23 安装包的独立 Web Consumer 通过 3/3 测试。
- Chrome WebGPU 验证零 Renderable 的 Canvas Swapchain Frame 后，继续完成多帧 PBR、Resize、
  资产 Fetch、异步 Pipeline、上传取消与资源释放测试。
- Shader Toolchain 的 `off`、`system`、`auto`、`download` 和旧缓存迁移测试通过；锁定工具链从
  干净缓存生成的 Shader Library、索引、内容 ID 与 Material 快照一致。

## 未完成的远端验收

Linux 共享/静态、远端安装 Consumer 和基于已推送固定提交的不可变 Release Candidate 尚未执行。
本记录不把这些项目或 0.23.0 正式发布标记为完成。
