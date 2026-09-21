<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader

## 定位

Shader 是离线生成的阶段入口。跨后端资产同时保存 SPIR-V 与 WGSL：Vulkan 使用 SPIR-V，WebGPU
使用 WGSL。Granit 核心库不在运行时编译或转换 Shader，也不向普通用户暴露原生 Shader 对象。
目前公共 Vulkan 路径支持 Vertex、Fragment 和 Compute；WebGPU MVP 支持 Vertex 和 Fragment。

## C API

发布资产使用 `.grshlib` 和 `granit_shader_create_from_library()`；调用方只提交 Library 与内容 ID，
由 Renderer 选择后端载荷。Material 同样通过稳定内容 ID 引用 Shader Library。

`granit_shader_create` 用于直接代码输入、底层接口契约测试及少量内部代码。它一次接收一种代码
格式，不提供跨后端自动选择。`.grshaderobj` 及其 sidecar 属于 AssetTools 私有中间结果，Core
公共 API 不读取该格式。

仓库测试所需的清单和同名 sidecar 由 `granit_test_shader_assets` 目标生成到构建目录；源码目录只
保存输入表示。正式资产使用 HLSL Library 源清单和 `granit_add_hlsl_shader_library` 构建，变体
Define 会排序并进入缓存身份，避免参数顺序造成重复缓存或宏变化误命中旧产物。

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
    renderer,
    granit::shader_desc{
        .stage = granit::shader_stage::vertex,
        .code_format = granit::shader_code_format::spirv,
        .code = std::span<const std::byte>{spirv_data, spirv_size},
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
