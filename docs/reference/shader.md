<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader

## 定位

Shader 是离线生成的阶段入口。跨后端资产同时保存 SPIR-V 与 WGSL：Vulkan 使用 SPIR-V，WebGPU
使用 WGSL。Granit 核心库不在运行时编译或转换 Shader，也不向普通用户暴露原生 Shader 对象。
目前公共 Vulkan 路径支持 Vertex、Fragment 和 Compute；WebGPU MVP 支持 Vertex 和 Fragment。

## C API

发布资产优先使用 `.grshlib` 和 `granit_shader_create_from_library()`；调用方只提交 Library 与内容
ID，由 Renderer 选择后端载荷。`.grshaderobj` 加单个 sidecar 的入口保留给工具、测试和过渡代码，
Core 会校验清单、当前 Renderer 后端与载荷摘要，但不执行文件 I/O。

```c
granit_shader_asset_desc asset = GRANIT_SHADER_ASSET_DESC_INIT;
asset.manifest_data = manifest_bytes;
asset.manifest_size = manifest_size;
asset.sidecar_data = sidecar_bytes;
asset.sidecar_size = sidecar_size;

granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_create_from_asset(renderer, &asset, &shader);
```

Vulkan 提供同名 `.grshaderobj.spv`，浏览器 WebGPU 提供 `.grshaderobj.wgsl`。清单损坏、缺少匹配变体、
能力不足或摘要不一致都会明确失败。成功返回后不再引用输入字节。

Material 通过稳定内容 ID 引用 Shader Library。`granit_shader_create` 只用于直接代码输入、底层
接口契约测试及少量内部嵌入代码；它与 Library 和单资产入口最终使用同一套后端 Shader 创建实现。
`.grshaderobj` 是清单，不能单独替代配套的 SPIR-V 或 WGSL 文件。

仓库测试所需的清单和同名 sidecar 由 `granit_test_shader_assets` 目标生成到构建目录；源码目录只
保存输入表示。发布资产仍可作为已验证制品提交。HLSL 变体通过可重复的 `--define NAME=VALUE`
生成，定义会排序并进入缓存身份，避免参数顺序造成重复缓存或宏变化误命中旧产物。CMake 调用方
可使用 `granit_add_hlsl_shader_object` 声明源码、Stage、入口和 `DEFINES`，并把三个输出作为构建
依赖。

调用方需要建立资产索引或选择变体时，可在不创建 Renderer 和 Shader 的情况下检查清单：

```c
granit_shader_asset_info info = GRANIT_SHADER_ASSET_INFO_INIT;
granit_result result = granit_shader_asset_inspect(manifest_bytes, manifest_size, &info);
if (result == GRANIT_SUCCESS && info.entry_point_length > 0) {
  char entry_point[64];
  info.entry_point = entry_point;
  info.entry_point_capacity = sizeof(entry_point);
  result = granit_shader_asset_inspect(manifest_bytes, manifest_size, &info);
}
```

第一次调用返回内容 ID、缓存键、Stage、入口点长度和至多两个当前格式变体摘要；第二次调用可复制
入口点。变体包含 Backend、代码格式、Profile、Required Features、载荷大小和摘要。接口完整校验
清单，不持有输入或输出内存；入口点容量不足返回 `GRANIT_ERROR_INVALID_ARGUMENT`。

直接描述入口仍适合内建 Shader、测试和自行管理载荷的调用方：

```c
granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
desc.stage = GRANIT_SHADER_STAGE_VERTEX;
desc.code_format = GRANIT_SHADER_CODE_FORMAT_SPIRV;
desc.code = spirv_bytes;
desc.code_size = spirv_size;
desc.entry_point = "main";
desc.entry_point_length = 4;

granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_create(renderer, &desc, &shader);
```

`code_format` 明确说明 `code` 是 SPIR-V 还是 UTF-8 WGSL。SPIR-V 不要求四字节对齐，但长度必须
是 4 的倍数；WGSL 不能包含嵌入零字符。直接代码格式与 Renderer 不匹配时返回
`GRANIT_ERROR_UNSUPPORTED`。函数返回后不再引用输入内存，入口点和 WGSL 均使用显式字节长度。
需要跨后端自动选择时应使用 Shader Library，不能在直接描述中同时传入两种代码。

## C++ API

```cpp
granit::shader shader;
const auto result = shader.initialize(
    renderer.native_handle(),
    granit::shader_desc{
        .stage = granit::shader_stage::vertex,
        .code_format = granit::shader_code_format::spirv,
        .code = std::span<const std::byte>{spirv_data, spirv_size},
    });
```

`granit::shader` 不可复制、可以移动，析构时自动销毁。若需要可靠处理销毁结果，可显式调用
`reset()`。

需要拥有检查结果时可使用 `granit::inspect_shader_asset()`。其输出字符串和变体数组由调用方对象
拥有，不依赖输入清单的生命周期。

## 校验与限制

- SPIR-V 最小长度为五个 32 位 word，最大为 64 MiB。
- 运行时检查长度、Magic Number、阶段和入口点等低成本约束。
- 完整 SPIR-V 校验和源代码诊断属于离线编译工具职责。
- WGSL 最大为 64 MiB，不能包含嵌入零字符；浏览器运行时不启动 Tint。
- 当前 Shader 只能用于后续 Pipeline，尚未提供独立执行或原生 Vulkan 互操作。
- 公开句柄会校验类型、generation 和 Renderer domain。

详细设计见 [D-01](../plans/D-01-shader-input.md)、[D-02](../plans/D-02-shader-module.md)和
[S-23](../plans/S-23-0.8.0-runtime-shader-assets.md)。
