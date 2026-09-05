<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.5 迁移到 0.6

## 适用场景

本文说明使用 Granit 0.5.0 的项目升级到 0.6.0 时需要处理的源码和资产变化。0.x 尚未承诺
API/ABI 稳定，升级后应重新编译应用及全部 Granit component。

## Shader 资产

0.6.0 将确定性 Shader 资产拆为后端无关清单和平台载荷：

```text
example.granit-shader
example.wgsl
example.spv
```

0.5.0 的单文件内嵌格式不再读取。请使用 0.6.0 的 ShaderTools 从 WGSL、HLSL 或 GLSL 源码重新
生成资产，不要复制或手工修改旧缓存。发布时可以只保留目标后端需要的 sidecar，但清单必须与
实际载荷一致。

## 能力与变体选择

应用可以通过 Renderer 查询实际 Shader 能力，并按后端、portable 档位和必需特性选择兼容变体。
找不到兼容变体时会明确返回 `GRANIT_ERROR_UNSUPPORTED`，调用方应停止创建 Shader 或选择较低
能力档位，不能假定任意 SPIR-V 或 WGSL 都可在当前设备运行。

构建工具可以查询内置的 `vulkan-portable` 与 `webgpu-portable` 目标能力。请求目标不支持的特性
会在写入资产前失败，应调整源码、特性声明或导出目标。

## 工具链

官方 Windows/Linux 离线工具链包锁定 DXC、glslang 与 Tint，并提供许可证和 SHA-256 清单。
严格可复现构建应使用锁定包；本地开发可以使用满足兼容策略的工具版本，但工具身份会进入缓存键，
因此不同版本不会误用彼此的缓存结果。

下载与校验方式见[Shader 工具链包清单](../reference/shader-toolchain-package.md)。

## ABI 与重新构建

0.6.0 为 Core C ABI 兼容新增 Shader 能力查询与变体选择导出，没有删除或改名既有导出。由于项目
仍处于 0.x，升级时仍必须清理旧构建目录，并使用同一套 0.6.0 头文件、动态库及可选 component
重新配置和编译。

## 验证

升级后至少验证：

1. 所有 Shader 资产均由 0.6.0 ShaderTools 重新生成。
2. 发布目录包含清单声明的全部 sidecar，且摘要校验通过。
3. 应用根据 Renderer 能力选择变体，并处理 `GRANIT_ERROR_UNSUPPORTED`。
4. C/C++ Consumer 不再加载 0.5.0 动态库或旧 Shader 缓存。
