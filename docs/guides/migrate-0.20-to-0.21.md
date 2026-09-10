<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.20 迁移到 0.21

0.21.0 将运行时 Shader 交付单元从 `.grshader` 清单和后端 sidecar 改为 `.grshlib`，并让 Material
直接引用 Shader Library。项目仍处于 0.x；Consumer 应重新编译、重建材质与 Shader 资产，并把
CMake 请求版本更新为 0.21。

## 改用 Shader Library

用 `granit_shader_tool library` 在离线构建中把材质引用的 Shader Asset 链接为 Library：

```powershell
granit_shader_tool library `
  --asset path/to/standard.vert.grshader `
  --asset path/to/standard.frag.grshader `
  --target all `
  --output path/to/standard.grshlib
```

`--target all` 同时保存 Vulkan SPIR-V 和 WebGPU WGSL；按平台发包时可改用 `vulkan` 或 `webgpu`
裁剪。`.grshader` 与 sidecar 是工具链中间输入，不再复制到运行时或安装目录。

应用读取或映射完整 `.grshlib` 后创建 Library。归档内存必须保持地址和内容不变，直到 Material、
由 Library 创建的 Shader 和 Pipeline 都释放，并成功销毁 Library：

```c
granit_shader_library_desc library_desc = GRANIT_SHADER_LIBRARY_DESC_INIT;
library_desc.archive_data = library_bytes;
library_desc.archive_size = library_size;

granit_shader_library library = GRANIT_NULL_HANDLE;
granit_result result =
    granit_shader_library_create(renderer, &library_desc, &library);
```

底层直接 Shader 用户可以用 `granit_shader_create_from_library` 按内容 ID 取得 Shader。Renderer
负责选择当前后端载荷；应用不查询 backend 或自行选择 `.spv`、`.wgsl`。

`granit_shader_desc` 也已收敛为单一代码输入：删除 `wgsl` 与 `wgsl_length`，统一使用 `code`、
`code_size` 和 `code_format`。直接创建一次只接收 SPIR-V 或 WGSL；需要跨后端自动选择时使用
Shader Library。

## 更新 Material 创建

0.20 的 `granit_material_shader_resolver`、resolver user data 和回调已删除。把创建描述改为设置
Library 句柄：

```c
granit_material_desc material_desc = GRANIT_MATERIAL_DESC_INIT;
material_desc.archive_data = material_bytes;
material_desc.archive_size = material_size;
material_desc.shader_library = library;
```

Material 保留 Library 引用，但不拥有 Library 或归档内存。销毁顺序应为 Material、直接创建的
Shader/Pipeline、Library，最后释放 `.grshlib` 字节。

`.grmat` 已升级到 v5。源 JSON 必须用 `binding_groups` 明确声明连续的 `frame`、`material`、
`object`、`lighting` 绑定组；不能再依赖 Pass 名称推断布局。使用 0.21 的 Material Tool 重新构建
所有旧归档。

## 调整变体和坐标代码

Pass 和 Feature 只表达会改变 Shader、绑定布局、顶点输入或固定 Pipeline 状态的静态功能。颜色、
粗糙度、Texture View 和 Sampler 等继续通过 `granit_material_update` 更新；更新它们不会创建新的
Pipeline。纹理功能发生变化时选择已有的 `pbr_texture_mask` 变体，同一功能内更换贴图只更新句柄。

RenderPipeline 接收统一的深度 `0..1` 裁剪空间，Canvas 使用左上原点。删除按 Vulkan/WebGPU 翻转
View Projection 的应用代码；Renderer 保持公开的正面绕序语义，Canvas 在内部转换像素投影。
纹理上传或外部纹理的 UV 方向仍应在对应资源路径处理。

## 更新安装资产路径

`RenderPipeline` component 的标准资产现在是：

- `materials/pbr_standard.grmat`
- `libraries/pbr_standard.grshlib`
- `environments/studio_small_03.grenv` 及其 manifest

`${granit_RENDER_PIPELINE_ASSET_DIR}/shaders/pbr` 不再属于安装契约。构建树和安装包使用同一最终资产
布局。
