<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.7 迁移到 0.8

## 适用场景

本文说明 0.7.0 Consumer 升级到 0.8.0 时需要处理的 Shader 与 Material 资产变化。Granit 仍处于
0.x，升级后应使用同一套 SDK 头文件、库与运行时组件重新编译应用。

## Shader Asset

`.grshader` 清单升级为自描述阶段、入口和稳定内容 ID。运行时可直接消费调用方读入的清单与当前
后端 sidecar：

- Vulkan 部署 `.grshader` 与 `.grshader.spv`。
- 浏览器 WebGPU 部署 `.grshader` 与 `.grshader.wgsl`。
- 同一发布包面向两端时可同时部署两个 sidecar。

调用 `granit_shader_create_from_asset` 后，输入字节即可释放。旧清单必须用 0.8.0 ShaderTools
重新生成；运行时不会猜测旧布局。

已有且经过验证的 SPIR-V/WGSL 对可用 `granit_shader_tool pack` 封装。HLSL、GLSL 或 WGSL 源码仍
建议通过对应 `compile-*` 命令生成，以保留完整工具身份和可复现缓存信息。

## Material 归档

`.grmat` 升级到格式 v4，归档不再内嵌 SPIR-V/WGSL，而是保存 Shader Asset 内容 ID。源 JSON 中
每个 Shader 改为引用清单：

```json
"shaders": [
  {"asset": "standard.vert.grshader"},
  {"asset": "standard.frag.grshader"}
]
```

使用 0.8.0 `granit_material_tool build` 重新生成所有 `.grmat`。创建 Material 时在
`granit_material_desc` 提供 `shader_resolver` 和 `shader_resolver_user_data`；resolver 根据内容 ID、
实际后端和 portable 档位返回清单及匹配 sidecar。Material 的 Pipeline 延迟创建，因此 resolver
状态必须存活到 Material 销毁。

Granit 不负责路径、异步 I/O、资产数据库或缓存。上游可以先异步读取资产，再通过同步 resolver
提供已经驻留的字节；依赖未就绪时返回 `GRANIT_ERROR_NOT_READY`。

## 验证结果

升级后至少验证：

1. 清理旧构建目录并重新配置 0.8.0 SDK。
2. 用新版工具重新生成 `.grshader`、sidecar 和 `.grmat`。
3. Vulkan 包只保留 SPIR-V sidecar 时能够创建材质 Pipeline。
4. 浏览器 WebGPU 包只保留 WGSL sidecar 时能够创建同一材质 Pipeline。
5. 缺失或摘要不匹配的 sidecar 返回明确错误，不静默选择其他变体。

## 常见问题

旧 `.grmat` 不能直接加载。这是尚未稳定的 0.x 持久化格式变更，项目选择离线重建而不是在运行时
保留多套解析分支。
