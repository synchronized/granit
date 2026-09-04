<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-005：WebGPU 仅支持 Emscripten 浏览器

- 状态：已接受
- 日期：2026-09-04
- 取代：[ADR-004](ADR-004-webgpu-provider-loading.md)

## 背景

Granit 的桌面目标已经由 Vulkan 覆盖。继续维护桌面 Dawn 会引入独立 SDK、插件 ABI、动态库发现、
工具链匹配和较重的 CI 成本，而产品目标只要求浏览器 WebGPU。

## 决策

- 桌面平台只提供 Vulkan；WebGPU 只在 Emscripten 构建中提供。
- 浏览器 WebGPU 后端静态编入最终 Wasm 模块并直接使用 Emdawnwebgpu，不经过动态插件加载。
- 保留私有 HAL、统一 Registry、公共 WebGPU 后端枚举和后端无关资源/命令接口。
- 删除桌面 Dawn SDK、WebGPU 动态插件、插件 ABI、动态查找和对应 CI。
- `webgpu.h` 仍只允许出现在 Emscripten WebGPU 后端内部，不进入公共头文件或安装依赖。
- 历史 Dawn 验收 Record 保留为当时实施记录，但不再描述当前支持范围。

## 影响

- WebGPU 调用路径缩短为 Registry、HAL、Emscripten WebGPU 实现和浏览器 WebGPU。
- 普通桌面构建不再下载、编译或打包 Dawn，也不再产生 WebGPU Provider 动态库。
- 桌面不能选择 WebGPU，WebGPU 行为和图像验收转移到无头浏览器及真实浏览器。
- Android WebGPU 不再是既定方向；Android 首阶段仅规划 Vulkan，未来需求需重新决策。

## 替代方案

- **继续维护桌面 Dawn 插件**：便于原生调试，但维护成本与当前产品需求不匹配。
- **桌面静态链接 Dawn**：调用边界较短，但让 Vulkan 用户承担不必要的体积和工具链依赖。
- **使用 wgpu-native 替代 Dawn**：仍需维护桌面 WebGPU，不解决范围和 CI 成本问题。
