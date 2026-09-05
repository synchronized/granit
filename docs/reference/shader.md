<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader

## 定位

Shader 是离线生成的阶段入口。跨后端资产同时保存 SPIR-V 与 WGSL：Vulkan 使用 SPIR-V，WebGPU
使用 WGSL。Granit 核心库不在运行时编译或转换 Shader，也不向普通用户暴露原生 Shader 对象。
目前公共 Vulkan 路径支持 Vertex、Fragment 和 Compute；WebGPU MVP 支持 Vertex 和 Fragment。

## C API

发布资产优先使用 `.grshader`：调用方负责读取清单和当前平台 sidecar，Core 根据实际 Renderer
能力选择变体并校验内容摘要，不执行文件 I/O。

```c
granit_shader_asset_desc asset = GRANIT_SHADER_ASSET_DESC_INIT;
asset.manifest_data = manifest_bytes;
asset.manifest_size = manifest_size;
asset.sidecar_data = sidecar_bytes;
asset.sidecar_size = sidecar_size;

granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_create_from_asset(renderer, &asset, &shader);
```

Vulkan 提供同名 `.grshader.spv`，浏览器 WebGPU 提供 `.grshader.wgsl`。清单损坏、缺少匹配变体、
能力不足或摘要不一致都会明确失败。成功返回后不再引用输入字节。

直接描述入口仍适合内建 Shader、测试和自行管理载荷的调用方：

```c
granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
desc.stage = GRANIT_SHADER_STAGE_VERTEX;
desc.code = spirv_bytes;
desc.code_size = spirv_size;
desc.entry_point = "main";
desc.entry_point_length = 4;
desc.wgsl = wgsl_bytes;
desc.wgsl_length = wgsl_size;

granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_create(renderer, &desc, &shader);
```

`code` 是 SPIR-V 字节输入，不要求四字节对齐，但长度必须是 4 的倍数；`wgsl` 是 UTF-8 WGSL
字节输入。调用者若只面向单个后端，可以只提供该后端所需表示；跨后端资产应同时提供二者。
函数返回后不再引用输入内存。入口点和 WGSL 均使用显式字节长度，不包含结尾零字符。

## C++ API

```cpp
granit::shader shader;
const auto result = shader.initialize_asset(
    renderer.native_handle(),
    granit::shader_asset_desc{
        .stage = granit::shader_stage::vertex,
        .spirv = std::span<const std::byte>{spirv_data, spirv_size},
        .wgsl = wgsl_source,
    });
```

`granit::shader` 不可复制、可以移动，析构时自动销毁。若需要可靠处理销毁结果，可显式调用
`reset()`。

## 校验与限制

- SPIR-V 最小长度为五个 32 位 word，最大为 64 MiB。
- 运行时检查长度、Magic Number、阶段和入口点等低成本约束。
- 完整 SPIR-V 校验和源代码诊断属于离线编译工具职责。
- WGSL 最大为 64 MiB，不能包含嵌入零字符；浏览器运行时不启动 Tint。
- 当前 Shader 只能用于后续 Pipeline，尚未提供独立执行或原生 Vulkan 互操作。
- 公开句柄会校验类型、generation 和 Renderer domain。

详细设计见 [D-01](../plans/D-01-shader-input.md)、[D-02](../plans/D-02-shader-module.md)和
[S-23](../plans/S-23-0.8.0-runtime-shader-assets.md)。
