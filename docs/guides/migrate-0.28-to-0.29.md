<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.28 迁移到 0.29

0.29.0 把 Shader 逻辑名称写入 `.grshlib`，删除独立 `.grshidx.json` 与生成的内容 ID include。
这是 0.x 源码、C ABI 和 Shader Library 持久化格式的破坏性调整。升级后须重新配置 CMake、重新
编译使用者，并用 0.29 工具重新生成 Shader Library；0.28 的动态库、对象文件和 `.grshlib` 不能
与 0.29 混用。

## 重新生成 Shader Library

`.grshlib.json` 继续作为 HLSL-first 源清单。每个 Library 与 Shader 的 `name` 现在直接写入
schema 2 归档，构建不再输出 `.grshidx.json`：

```sh
granit_asset_tool shader build-library \
  --manifest assets/sources/shaders/example.grshlib.json \
  --toolchain path/to/granit-shader-tools \
  --cache build/shader-cache \
  --output build/example.grshlib
```

删除构建脚本中的 `--index`、索引文件安装规则和 `*_shader_ids.inc` 生成步骤。逻辑名称在单个
Library 内必须唯一；同一名称映射到不同 Shader 内容会使构建失败。

## 按逻辑名称创建 Shader

C++ 普通路径不再包含构建生成的内容 ID：

```cpp
granit::shader vertex;
if (const auto result = library.create_shader("example.vertex", vertex); !result.ok())
  return result;
```

原 `shader_library::create_shader(content_id, shader)` 改为名称明确的
`create_shader_by_content_id(content_id, shader)`，仅用于缓存和资产系统等确实持有内容身份的
高级路径。

C API 的 `granit_shader_create_from_library` 已拆分为：

- `granit_shader_create_from_library_name`：普通运行时路径；
- `granit_shader_create_from_library_content_id`：按内容 ID 的高级路径。

旧函数不保留兼容别名，所有使用者必须重新编译。

## 更新 Material 构建

Material Builder 现在直接读取一个或多个 `.grshlib`：

```sh
granit_asset_tool material build material.grmat.json \
  --shader-library build/example.grshlib \
  --output build/material.grmat
```

把 CLI 的 `--shader-index` 替换为 `--shader-library`。C API 将
`granit_asset_tools_material_shader_index` 和 `shader_indices` 改为
`granit_asset_tools_material_shader_library` 和 `shader_libraries`；输入内容从 UTF-8 JSON 变为
`.grshlib` 二进制字节。C++ `material::build_desc::shader_libraries` 对应接收字节 span。

`granit_asset_tools_shader_source_library_desc` 删除 `index_path`，
`granit_asset_tools_shader_index_find_content_id` 与 C++ `find_index_content_id` 已移除。需要解析逻辑
名称的工具应直接读取 Shader Library，普通应用使用运行时按名称创建接口。

## 资产兼容性

Shader 内容 ID 的计算方式没有改变。已有 `.grmat` 仍按内容 ID 引用 Shader，但它必须与用 0.29
工具重新生成且包含对应内容的 `.grshlib` 配套使用。建议在升级时一起重建 Material，以便构建阶段
校验 Library 名称和 Shader 逻辑名称。

当前接口和格式见 [Shader Library](../reference/shader-library.md)、
[Material](../reference/material.md) 与 [AssetTools](../reference/asset-tools.md)。
