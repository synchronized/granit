<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-13 S-37L 本地 Release 验收

## 结果

S-37L 在提交 `ba9338b` 上完成 Windows 与浏览器本地 Release 验收。共享、静态和 Emscripten
构建均使用 HLSL-first 资产入口，并验证内建 Shader Library 快照、内容 ID、公共 ABI、安装边界、
Vulkan 与 WebGPU 渲染路径。

真实 Chrome WebGPU 复验曾发现 Tone Mapping 常量块布局不一致：HLSL 的 `uint3` 填充转换成
WGSL 后按 16 字节对齐，使浏览器要求 48 字节，而 CPU 绑定 32 字节。提交 `7143085` 将填充改为
三个标量并重建 Library，修复后 Debug 与 Release 浏览器验收均通过。

## 本地验证

- Windows VS2022 共享 Release：完整构建与 84/84 测试通过。
- Windows VS2022 静态 Release：完整构建与 78/78 测试通过。
- Emscripten Release：完整构建与 3/3 宿主测试通过。
- Chrome WebGPU Release：多帧渲染、像素、质量与光照切换、输入、Resize、资产 Fetch、资源释放、
  原生异步 Pipeline、上传取消与回滚、外部 Buffer 缺失诊断全部通过。

## 未完成的远端验收

当前机器没有可用 WSL 发行版或 Docker Linux 服务。Linux、远端 SDK 和基于已推送固定提交的
不可变 Release Candidate 验收继续由 S-37G 统一执行；本记录不把这些项目标记为通过。
