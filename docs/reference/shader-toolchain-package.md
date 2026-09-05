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

已有锁定工具和许可证材料时，使用组包脚本建立统一目录：

```cmake
cmake \
  -DSTAGE=<新的归档根目录> \
  -DGENERATOR=cmake/generate_shader_toolchain_manifest.cmake \
  -DDXC=<dxc 路径> \
  -DGLSLANG=<glslangValidator 路径> \
  -DTINT=<tint 路径> \
  -DDXC_VERSION=<版本> \
  -DGLSLANG_VERSION=<版本> \
  -DDAWN_VERSION=<版本> \
  -DTINT_REVISION=<源码修订> \
  "-DDXC_LICENSE_FILES=<DXC 许可证列表>" \
  "-DGLSLANG_LICENSE_FILES=<glslang 许可证列表>" \
  "-DDAWN_LICENSE_FILES=<Dawn/Tint 许可证列表>" \
  "-DRUNTIME_FILES=<必要运行库列表>" \
  -P cmake/package_shader_toolchain.cmake
```

三组许可证列表均不能为空。脚本将工具标准化到 `bin/`，将许可证分别放入组件子目录，其他显式
运行库也放入 `bin/`。目标目录必须尚不存在；组包在临时目录完成后才原子重命名，失败不会留下
可被误认为成功产物的目标目录。

如果发布流程已经自行准备好完整目录，也可直接调用底层清单生成器：

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

完整 Vulkan SDK 不是工具链包的一部分。它可以作为本地开发时查找 DXC 和 glslangValidator 的
可选来源，但官方可复现构建应使用经过清单验证的精简工具链包。

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
