<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-004：WebGPU 实现与加载策略

- 状态：提议
- 日期：2026-08-26

## 背景

Granit 需要同时支持桌面原生 WebGPU 与 Emscripten 浏览器路径，同时保持 WebGPU 类型和实现依赖
位于动态库内部。WebGPU Native C API 尚不能假设任意实现和版本之间都具备 Vulkan Loader 级别的
二进制兼容性，因此头文件、实现库和浏览器绑定必须作为匹配组合管理。

## 决策

- 桌面原型使用 Dawn Native；浏览器使用同一 Dawn 发布配套的 Emdawnwebgpu。
- 只使用锁定 Dawn 修订提供的 `webgpu.h` C API，不依赖非稳定的 `webgpu_cpp.h`。
- `webgpu.h` 只允许在 `src/backend/webgpu` 内包含，不进入公共头、安装导出或传递依赖。
- 桌面端通过内部函数表和平台动态库 API 加载匹配的 Dawn 共享库。库缺失、符号缺失或版本探测
  失败统一视为后端不可用，不影响 Vulkan 后端启动。
- Emscripten 端在最终链接时接入 Emdawnwebgpu port，并向同一内部调用层提供静态绑定；浏览器路径
  不尝试加载桌面共享库。
- 原型暂定 Dawn `v20260720.160313`、修订 `0bc38adde72b79013536f8ce354b639ae19ae195`，以及
  该发布验证的 emsdk `5.0.6`。正式锁定前必须完成 Windows、Linux 和浏览器 smoke test。
- 不把 Dawn 预编译产物提交进源码仓库。官方桌面发布包仅用于评估；Granit 在锁定镜像中从同一
  修订构建 monolithic shared library，生成校验和并通过缓存或镜像分发，本地允许显式指定包目录。
- 首版只承诺匹配版本的 Dawn，不在运行时自动替换为 wgpu-native。后续只有在标准 C ABI、扩展和
  异步语义验证兼容后，才增加其他实现适配器。

## 影响

- 桌面安装可以保持 Dawn 可选；没有 Dawn 时仍可使用 Vulkan。
- 原生与浏览器共享主要 WebGPU 调用代码，但加载和事件推进由平台适配层分别实现。
- Dawn 升级需要同步验证头文件、共享库、Emdawnwebgpu、Shader 工具链和 CI 镜像。
- 动态加载降低强制运行时依赖，但不能保证用户提供的任意 `webgpu` 库都可替换锁定 Dawn。
- 自建 C ABI 共享库隔离 Dawn 的 C++ 静态库工具链差异，但镜像维护和安全更新成本会上升。

## 替代方案

- **直接链接 Dawn**：接入简单，但会让核心库始终依赖 Dawn，暂不采用。
- **桌面使用 wgpu-native、浏览器使用 Emdawnwebgpu**：构建产物较易获得，但增加两套实现的版本和
  行为差异，首版不采用。
- **只使用 Emscripten WebGPU**：无法验证桌面后端和复用原生测试，拒绝。
- **运行时接受任意 WebGPU 实现**：当前跨实现 ABI 与扩展兼容依据不足，推迟。
