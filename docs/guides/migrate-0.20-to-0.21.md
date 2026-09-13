<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.20 迁移到 0.21

0.21.0 将运行时 Shader 交付单元从 `.grshader` 清单和后端 sidecar 改为 `.grshlib`，并让 Material
直接引用 Shader Library。项目仍处于 0.x；Consumer 应重新编译、重建材质与 Shader 资产，并把
CMake 请求版本更新为 0.21。

## 迁移统一 Surface 创建

0.21 删除了 `granit_surface_create_win32`、`granit_surface_create_xcb`、
`granit_surface_create_wayland` 和 `granit_surface_create_canvas`，不提供兼容别名。所有平台改用
`granit_surface_desc` 的带标签联合体和 `granit_surface_create`：

```c
granit_surface_desc surface_desc = GRANIT_SURFACE_DESC_INIT;
surface_desc.surface_type = GRANIT_SURFACE_TYPE_WIN32_BIT;
surface_desc.source.win32.instance = hinstance;
surface_desc.source.win32.window = hwnd;
granit_surface_create(renderer, &surface_desc, &surface);
```

C++ 将 `initialize_win32`、`initialize_xcb`、`initialize_wayland` 和 `initialize_canvas` 收敛为
`surface::initialize`，来源通过 `surface_desc::win32`、`xcb`、`wayland` 或 `canvas` 工厂表达：

```cpp
surface.initialize(
    renderer.native_handle(),
    granit::surface_desc::win32(hinstance, hwnd));
```

原生窗口、display 和 connection 必须保持有效直到 Surface 销毁。Canvas selector 只在创建调用
期间借用。

## 改用 Shader Library

为 HLSL 源码编写 `.grshlib.json` 清单，再用 `build-library` 在离线构建中生成 Library 和逻辑名称
索引：

```powershell
granit_shader_tool build-library `
  --manifest path/to/standard.grshlib.json `
  --dxc path/to/dxc --tint path/to/tint `
  --cache path/to/cache `
  --output path/to/standard.grshlib `
  --index path/to/standard.grshidx.json
```

目标后端在源清单中声明；`.grshaderobj` 与 sidecar 只存在于工具缓存，不复制到运行时或安装目录。

ShaderTools 的离线作者入口只接受 HLSL。删除调用方的 `shader_source_language` 选择以及直接导入
WGSL/SPIR-V 配对载荷、写入 Object、恢复 Object 缓存或链接 Object 的代码，改为一次调用
`granit_shader_tools_build_library_from_manifest`。单 Shader 编辑器诊断仍可使用 Compiler；输入固定为
HLSL，Compilation 可查询生成的 SPIR-V、WGSL 和 Reflection。工具二进制身份由 Library Builder
内部写入缓存键，不再通过公共 API 查询。

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

`granit_shader_create_from_asset`、`granit_shader_asset_inspect` 及对应 C++ 单 Object 包装已删除，
不保留兼容入口。原来直接部署 `.grshader` 与单个 sidecar 的代码应改为创建 Library，并按内容 ID
调用 `granit_shader_create_from_library`。

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
