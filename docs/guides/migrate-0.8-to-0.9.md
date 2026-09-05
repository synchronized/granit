<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.8 迁移到 0.9

0.9.0 将 Model Viewer 已验证的标准 PBR Shader 收敛为 RenderPipeline component 的公共资产。
公共 C/C++ API 与 `.grshader`、`.grmat` 格式没有变化，但 Granit 仍处于 0.x，Consumer 应重新编译。

## 更新包版本

将 CMake 请求更新为 0.9，并继续按实际需要声明 component：

```cmake
find_package(granit 0.9 CONFIG REQUIRED COMPONENTS RenderPipeline)
```

## 使用标准 PBR 资产

请求 RenderPipeline 后，可通过 `granit_RENDER_PIPELINE_ASSET_DIR` 获得安装资产根目录。标准资产位于
其 `shaders/pbr` 子目录，包含顶点、片元 `.grshader` 清单和当前发布支持的 `.spv`、`.wgsl`
sidecar。应用仍负责读取、嵌入或缓存文件，再把内存字节交给 Granit；Core 不接管文件 I/O。

自定义 PBR Shader 不受影响。若此前复制了 Model Viewer 的私有 Shader，建议改为公共标准资产，
或明确维护自己的 Binding 契约；不要继续依赖已经删除的 `model_viewer_pbr.*` Shader 文件。

## 验证

1. 清理旧构建目录并重新配置 0.9.0 SDK。
2. 确认 RenderPipeline component 已安装，且 `granit_RENDER_PIPELINE_ASSET_DIR` 存在。
3. 重新生成引用标准 PBR Shader 的材质包，并分别验证 Vulkan 与浏览器 WebGPU 目标 sidecar。
