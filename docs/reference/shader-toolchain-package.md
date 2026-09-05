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

三组许可证列表均不能为空。官方工作流从锁定的 DXC 与 glslang 源码标签下载完整许可证和第三方
声明，并在组包前校验其 SHA-256；Vulkan SDK 的总许可说明不能替代这些组件材料。脚本将工具
标准化到 `bin/`，将许可证分别放入组件子目录；显式运行库
在 Windows 放入 `bin/`，在 Unix 放入 `lib/`，以保留常见的相对运行库布局。目标目录必须尚不
存在；组包在临时目录完成后才原子重命名，失败不会留下可被误认为成功产物的目标目录。

Dawn/Tint 从源码静态构建时，还应先汇总其源码及已获取第三方依赖中的许可证：

```cmake
cmake \
  -DROOT=<Dawn 源码根目录> \
  -DOUTPUT=<临时目录>/Dawn-THIRD-PARTY-LICENSES.txt \
  -DCOMPONENT=Dawn-Tint \
  -P cmake/collect_license_bundle.cmake
```

汇总器递归收集 `LICENSE*`、`COPYING*` 和 `NOTICE*`，按相对路径稳定排序并保留来源标记。该机制
保证构建时实际存在的许可证材料进入归档，但不能代替工具升级时对上游再分发要求的人工复核。

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

仓库的 `Shader Toolchain Packages` 手动 Actions 工作流固定 Vulkan SDK 下载地址、归档 SHA-256、
Dawn 修订和全部工具版本。Windows 与 Linux 分别构建 Tint、组装精简目录、执行包内清单校验，
再以 `locked` 策略运行 HLSL/GLSL 双后端 ShaderTools 测试，最后上传带独立 SHA-256 文件的临时
Artifact。当前锁定产物已作为独立预发行版本发布；后续工具升级仍须先完成两平台远端验证和
许可证复核，再发布新标签，不能覆盖已有归档。

工作流分别缓存 Dawn 第三方源码、编译目标和最终 Tint/许可证产物。最终产物缓存键包含平台、
架构、编译器契约版本及 Tint 修订；命中时不再获取或编译 Dawn。该缓存只用于加速，组包后仍执行
清单和真实编译能力验证，不能替代可发布归档及其 SHA-256。

## 下载锁定工具链

Windows x64 与 Linux x64 可以显式运行下载脚本。脚本选择当前宿主归档、校验发布级 SHA-256，
原子解包并验证包内清单；已有目录只有再次通过校验才会复用：

```sh
cmake -DDESTINATION=<缓存目录> -P cmake/download_shader_toolchain.cmake
```

随后使用脚本输出的目录配置 ShaderTools，并同时声明锁定的 Tint 修订：

```sh
cmake -S . -B build \
  -DGRANIT_SHADER_TOOLCHAIN_ROOT=<缓存目录>/<脚本输出的工具链目录> \
  -DGRANIT_SHADER_TOOLCHAIN_POLICY=locked \
  -DGRANIT_TINT_REVISION=0bc38adde72b79013536f8ce354b639ae19ae195
```

工具链发布页为
[Shader Toolchain v20260720.160313](https://github.com/synchronized/granit/releases/tag/shader-toolchain-v20260720.160313-0bc38adde72b)。
