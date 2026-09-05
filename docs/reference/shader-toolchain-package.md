<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Shader 工具链包清单

Shader 工具链包使用 `shader-toolchain.json` 描述宿主平台归档中的工具、运行库和许可证文件。
生成器记录完整文件集合，验证器要求文件集合、大小和 SHA-256 全部一致；缺失、篡改、重复登记或
额外文件都会使验证失败。

## 目录与角色

工具包是独立的离线资产构建依赖，不进入 Granit 核心 SDK 或应用运行时。归档根目录通常包含：

```text
bin/                      # DXC、glslangValidator、Tint 及必要运行库
licenses/                 # 上游许可证和第三方声明
shader-toolchain.json     # 完整性清单
```

清单为每个文件记录相对路径、字节数、SHA-256 和角色：

- `tool`：显式传给生成器的编译工具；
- `license`：显式传给生成器的许可证材料；
- `runtime`：归档中的其余必要运行库或数据文件。

路径必须位于归档根目录内。清单本身不记录自身摘要，归档下载层应另外校验整个归档的 SHA-256。

## 生成

打包阶段准备完整目录后执行：

```cmake
cmake \
  -DSTAGE=<归档根目录> \
  -DOUTPUT=<归档根目录>/shader-toolchain.json \
  -DDXC_VERSION=<版本> \
  -DGLSLANG_VERSION=<版本> \
  -DDAWN_VERSION=<版本> \
  -DTINT_REVISION=<源码修订> \
  "-DTOOL_FILES=bin/dxc;bin/glslangValidator;bin/tint" \
  "-DLICENSE_FILES=licenses/DXC.txt;licenses/glslang.txt;licenses/Dawn.txt" \
  -P cmake/generate_shader_toolchain_manifest.cmake
```

`TOOL_FILES` 与 `LICENSE_FILES` 均不能为空，其中任一必需文件不存在都会失败。生成结果采用稳定路径
排序，并通过临时文件替换目标清单。

## 验证

下载、解包或发布前执行：

```cmake
cmake \
  -DSTAGE=<归档根目录> \
  -DMANIFEST=<归档根目录>/shader-toolchain.json \
  -P cmake/verify_shader_toolchain_manifest.cmake
```

验证成功只说明解包后的文件与清单一致。发布系统仍须校验归档摘要，官方 CI 仍须使用
`GRANIT_SHADER_TOOLCHAIN_POLICY=locked` 完成真实编译能力测试。

