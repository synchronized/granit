<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader Library

## 定位

Shader Library 是供运行时加载的确定性 `.grshlib` 容器。一个 Library 可以保存 Vulkan SPIR-V、
WebGPU WGSL 或两者，并按 Shader 内容 ID 组织阶段、入口、反射、能力要求和去重后的载荷。应用负责
读取或映射整个归档，Core 负责格式与摘要校验，不执行文件 I/O 或运行时源码编译。

`.grshader` 及其 sidecar 只作为离线链接器的中间输入，不属于安装或运行时资产。Material 只接收
Library 句柄，不再接收 Renderer 后端信息或 Shader resolver。RenderPipeline component 安装的
标准 Library 位于 `${granit_RENDER_PIPELINE_ASSET_DIR}/libraries/pbr_standard.grshlib`。

## 创建与所有权

```c
granit_shader_library_desc desc = GRANIT_SHADER_LIBRARY_DESC_INIT;
desc.archive_data = archive_bytes;
desc.archive_size = archive_size;

granit_shader_library library = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_library_create(renderer, &desc, &library);
```

Library 属于传入的 Renderer。创建时会完整校验归档布局、内容摘要、载荷摘要、记录顺序和引用关系。
未知格式版本返回 `GRANIT_ERROR_UNSUPPORTED`，损坏或非规范归档返回
`GRANIT_ERROR_INVALID_ARGUMENT`。

创建成功后，Granit 借用 `archive_data` 指向的完整归档，直到
`granit_shader_library_destroy()` 成功或父 Renderer 销毁。调用者必须保证这段内存的地址和内容在
此期间有效且不变。Library 销毁后句柄立即失效；重复销毁、跨 Renderer 使用、错误资源类型和旧
generation 均返回 `GRANIT_ERROR_INVALID_HANDLE`。销毁 Renderer 会级联释放其 Library 句柄。

## 检查与摘要

`granit_shader_library_inspect()` 不需要 Renderer，也不持有输入内存。它与创建使用相同的严格检查，
并返回内容摘要、后端位、Shader 数、变体数、去重载荷数和归档大小：

```c
granit_shader_library_info info = GRANIT_SHADER_LIBRARY_INFO_INIT;
granit_result result =
    granit_shader_library_inspect(archive_bytes, archive_size, &info);
```

已创建的 Library 可通过 `granit_shader_library_get_info()` 读取相同摘要。后端位只说明归档中包含
哪些载荷，不要求应用根据 Renderer 选择载荷。

## Shader 选择与缓存

调用方使用内容 ID 请求 Shader，Renderer 根据真实后端、Profile 和能力位选择兼容载荷：

```c
granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result =
    granit_shader_create_from_library(renderer, library, content_id, &shader);
```

Core 会在使用前再次校验所选载荷摘要。Library 内按内容 ID 缓存后端 Shader；重复请求返回不同的
公开 Shader 句柄，但共享同一个后端对象。每个返回句柄均由调用者通过 `granit_shader_destroy()`
释放。找不到内容 ID 返回 `GRANIT_ERROR_NOT_READY`，没有兼容后端变体或能力不足返回
`GRANIT_ERROR_UNSUPPORTED`。

Shader 句柄及由它创建的 Pipeline 会保留缓存对象。任一引用尚未释放时，
`granit_shader_library_destroy()` 返回 `GRANIT_ERROR_RESOURCE_IN_USE`，Library 句柄仍然有效，调用方
可在释放依赖后重试销毁。

`granit_material_desc.shader_library` 把 Library 交给 Material。Material 会立即保留引用，即使尚未
按需创建 Pipeline，也必须先销毁 Material 才能销毁 Library。Material 归档仍只保存稳定内容 ID，
不携带后端枚举或载荷。

## C++ API

```cpp
granit::shader_library library;
const auto result = library.initialize(renderer.native_handle(), archive);

granit::shader_library_info info;
if (result.ok())
  library.get_info(info);

granit::shader shader;
library.create_shader(content_id, shader);
```

`granit::shader_library` 不可复制、可以移动，析构时自动销毁。它沿用 C API 的借用规则，不复制或
拥有归档字节。`create_shader()` 创建的 Shader 仍在使用 Library，必须先销毁这些 Shader，才能销毁
Library。需要在创建前离线检查时可使用 `granit::inspect_shader_library()`。

格式和架构决策见 [S-37](../plans/S-37-0.21.0-shader-library-and-material-boundary.md) 与
[ADR-006](../decisions/ADR-006-shader-library-runtime-asset.md)。
