<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-004：WebGPU 实现与加载策略

- 状态：提议
- 日期：2026-08-26

## 背景

Granit 需要同时支持桌面原生 WebGPU、Android 与 Emscripten 浏览器路径，同时保持 WebGPU 类型和
实现依赖位于核心公共 ABI 之外。WebGPU Native C API 尚不能假设任意实现和版本之间都具备 Vulkan
Loader 级别的二进制兼容性，因此不能把第三方 WebGPU ABI 直接作为 Granit 的后端插件边界。

## 决策

- 桌面和 Android 原型使用 Dawn Native；浏览器使用同一 Dawn 发布配套的 Emdawnwebgpu。
- 只使用锁定 Dawn 修订提供的 `webgpu.h` C API，不依赖非稳定的 `webgpu_cpp.h`。
- `webgpu.h` 只允许在 WebGPU 后端实现内包含，不进入公共头、核心安装导出或传递依赖。
- 桌面端构建 Granit 自有的 WebGPU 后端插件，插件内部静态链接匹配的 Dawn。核心库只加载并校验
  Granit 定义的版本化内部插件 ABI，不逐个解析 `wgpu*` 符号，也不接受任意 WebGPU Provider。
- Android 默认将同一 WebGPU 后端实现和 Dawn 静态并入 Granit 的平台库，避免额外原生库装载与
  APK ABI 管理；确有模块化部署需求时，才构建独立 Android 后端 `.so`。
- Emscripten 端在最终链接时接入 Emdawnwebgpu port，并将同一后端契约静态绑定进最终模块；浏览器
  路径不使用原生插件加载器。
- 原型暂定 Dawn `v20260720.160313`、修订 `0bc38adde72b79013536f8ce354b639ae19ae195`，以及
  该发布验证的 emsdk `5.0.6`。正式锁定前必须完成 Windows、Linux 和浏览器 smoke test。
- 不把 Dawn 预编译产物提交进源码仓库。官方桌面发布包和自建 Dawn 共享库只用于 ABI、导出与设备
  原型评估；正式构建从锁定修订生成与插件工具链匹配的 Dawn 静态库，并通过缓存或镜像复用。
- 首版只承诺匹配版本的 Dawn，不在运行时自动替换为 wgpu-native。后续只有在标准 C ABI、扩展和
  异步语义验证兼容后，才增加其他实现适配器。

## 影响

- 桌面安装可以保持 WebGPU 插件可选；没有插件时仍可使用 Vulkan。
- Dawn 的 C/C++ ABI、运行库和扩展被封装在插件内部，核心库只承担自有插件 ABI 的兼容性。
- 原生、Android 与浏览器共享主要 WebGPU 后端代码，但装载、事件推进和 Surface 由平台适配层实现。
- Dawn 升级需要同步验证头文件、桌面插件、Android 静态库、Emdawnwebgpu、Shader 工具链和 CI
  镜像。
- 桌面插件必须与其静态 Dawn 使用同一工具链和运行库构建，产物体积和镜像维护成本会上升。
- Android 和 Emscripten 的静态路径不具备运行时替换能力，但部署和符号解析更简单。

## 替代方案

- **核心库直接静态链接 Dawn**：接入简单，但会让所有桌面构建和 Vulkan 用户承担 Dawn 体积与
  工具链依赖；仅在 Android 和 Emscripten 的平台构建中采用。
- **核心库动态加载 `webgpu_dawn`**：已验证共享库构建和基础符号加载，但会把不稳定的第三方 ABI
  作为运行时边界并维护大量函数指针，桌面正式方案不采用。
- **桌面使用 wgpu-native、浏览器使用 Emdawnwebgpu**：构建产物较易获得，但增加两套实现的版本和
  行为差异，首版不采用。
- **只使用 Emscripten WebGPU**：无法验证桌面后端和复用原生测试，拒绝。
- **运行时接受任意 WebGPU 实现**：当前跨实现 ABI 与扩展兼容依据不足，推迟。
