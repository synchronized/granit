<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader

## 定位

Shader 是离线生成的 SPIR-V 阶段入口。Granit 核心库不编译 GLSL/HLSL，也不向普通用户暴露
`VkShaderModule`。目前支持 Vertex、Fragment 和 Compute 三种阶段。

## C API

```c
granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
desc.stage = GRANIT_SHADER_STAGE_VERTEX;
desc.code = spirv_bytes;
desc.code_size = spirv_size;
desc.entry_point = "main";
desc.entry_point_length = 4;

granit_shader shader = GRANIT_NULL_HANDLE;
granit_result result = granit_shader_create(renderer, &desc, &shader);
```

`code` 是字节输入，不要求四字节对齐，但长度必须是 4 的倍数。Granit 在创建调用内复制并转换
输入；函数返回后不再引用该内存。入口点不是零结尾字符串契约，实际长度由
`entry_point_length` 明确给出。

## C++ API

```cpp
granit::shader shader;
const auto result = shader.initialize(
    renderer.native_handle(),
    {.stage = granit::shader_stage::vertex, .code = std::span<const std::byte>{data, size}});
```

`granit::shader` 不可复制、可以移动，析构时自动销毁。若需要可靠处理销毁结果，可显式调用
`reset()`。

## 校验与限制

- SPIR-V 最小长度为五个 32 位 word，最大为 64 MiB。
- 运行时检查长度、Magic Number、阶段和入口点等低成本约束。
- 完整 SPIR-V 校验和源代码诊断属于离线编译工具职责。
- 当前 Shader 只能用于后续 Pipeline，尚未提供独立执行或原生 Vulkan 互操作。
- 公开句柄会校验类型、generation 和 Renderer domain。

详细设计见 [D-01](../plans/D-01-shader-input.md) 和 [D-02](../plans/D-02-shader-module.md)。
