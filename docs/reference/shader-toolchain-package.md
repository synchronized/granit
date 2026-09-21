<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# AssetTools Shader 工具链包清单

本文件描述 AssetTools 使用的离线 Shader 构建依赖包，不是 Granit 的 SDK 组件，也不是应用运行时
依赖。工具链包使用 `shader-toolchain.json` 描述宿主平台归档中的工具、运行库和许可证文件。生成器
记录完整文件集合，验证器要求文件集合、大小和 SHA-256 全部一致；缺失、篡改、重复登记或额外文件
都会使验证失败。

## 目录与角色

工具包是独立的离线资产构建依赖，不进入 Granit 核心 SDK 或应用运行时。归档根目录通常包含：

```text
bin/                      # DXC、Tint 及必要运行库
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
  -DGENERATOR=cmake/toolchain/generate_shader_toolchain_manifest.cmake \
  -DDXC=<dxc 路径> \
  -DTINT=<tint 路径> \
  -DDXC_VERSION=<版本> \
  -DDAWN_VERSION=<版本> \
  -DTINT_REVISION=<源码修订> \
  "-DDXC_LICENSE_FILES=<DXC 许可证列表>" \
  "-DDAWN_LICENSE_FILES=<Dawn/Tint 许可证列表>" \
  "-DRUNTIME_FILES=<必要运行库列表>" \
  -P cmake/toolchain/package_shader_toolchain.cmake
```

两组许可证列表均不能为空。官方工作流从锁定的 DXC 源码标签下载完整许可证和第三方声明，
并在组包前校验其 SHA-256；Vulkan SDK 的总许可说明不能替代这些组件材料。脚本将工具
标准化到 `bin/`，将许可证分别放入组件子目录；显式运行库
在 Windows 放入 `bin/`，在 Unix 放入 `lib/`，以保留常见的相对运行库布局。目标目录必须尚不
存在；组包在临时目录完成后才原子重命名，失败不会留下可被误认为成功产物的目标目录。

Dawn/Tint 从源码静态构建时，还应先汇总其源码及已获取第三方依赖中的许可证：

```cmake
cmake \
  -DROOT=<Dawn 源码根目录> \
  -DOUTPUT=<临时目录>/Dawn-THIRD-PARTY-LICENSES.txt \
  -DCOMPONENT=Dawn-Tint \
  -P cmake/assets/collect_license_bundle.cmake
```

汇总器递归收集 `LICENSE*`、`COPYING*` 和 `NOTICE*`，按相对路径稳定排序并保留来源标记。该机制
保证构建时实际存在的许可证材料进入归档，但不能代替工具升级时对上游再分发要求的人工复核。

如果发布流程已经自行准备好完整目录，也可直接调用底层清单生成器：

```cmake
cmake \
  -DSTAGE=<归档根目录> \
  -DOUTPUT=<归档根目录>/shader-toolchain.json \
  -DDXC_VERSION=<版本> \
  -DDAWN_VERSION=<版本> \
  -DTINT_REVISION=<源码修订> \
  "-DTOOL_FILES=bin/dxc;bin/tint" \
  "-DLICENSE_FILES=licenses/DXC.txt;licenses/Dawn.txt" \
  -P cmake/toolchain/generate_shader_toolchain_manifest.cmake
```

`TOOL_FILES` 与 `LICENSE_FILES` 均不能为空，其中任一必需文件不存在都会失败。生成结果采用稳定路径
排序，并通过临时文件替换目标清单。

完整 Vulkan SDK 不是工具链包的一部分。它可以作为本地开发时查找 DXC 的
可选来源，但官方可复现构建应使用经过清单验证的精简工具链包。

## 验证

下载、解包或发布前执行：

```cmake
cmake \
  -DSTAGE=<归档根目录> \
  -DMANIFEST=<归档根目录>/shader-toolchain.json \
  -P cmake/toolchain/verify_shader_toolchain_manifest.cmake
```

验证成功只说明解包后的文件与清单一致。发布系统仍须校验归档摘要，官方 CI 仍须使用
`GRANIT_SHADER_TOOLCHAIN_POLICY=locked` 完成真实编译能力测试。

发布工作流可以使用本清单验证归档内容，但工作流、缓存和发布标签不是工具链包格式的一部分。
发布系统仍须在包外校验归档 SHA-256，并在 `locked` 策略下完成真实编译能力测试。

## 下载锁定工具链

Windows x64 与 Linux x64 可以显式运行下载脚本。脚本选择当前宿主归档、校验发布级 SHA-256，
原子解包并验证包内清单；已有目录只有再次通过校验才会复用：

```sh
cmake -DDESTINATION=<缓存目录> -P cmake/toolchain/download_shader_toolchain.cmake
```

也可以让可安装的 CMake 模块按模式完成查找或下载：

```sh
cmake -S . -B build \
  -DGRANIT_SHADER_TOOLCHAIN_MODE=download \
  -DGRANIT_SHADER_TOOLCHAIN_CACHE_DIR=<缓存目录> \
  -DGRANIT_SHADER_TOOLCHAIN_POLICY=locked
```

`system`（默认）不访问网络，`off` 完全禁用 Shader 工具链，`auto` 仅在本机工具不完整时下载，
`download` 固定使用锁定发布包。显式 `GRANIT_SHADER_TOOLCHAIN_ROOT` 始终优先。安装 Consumer 在
请求 `AssetTools` component 后可 `include("${granit_SHADER_TOOLCHAIN_MODULE}")` 并调用
`granit_find_shader_toolchain()`。

锁定工具链发布页见 [Granit Releases](https://github.com/synchronized/granit/releases)。
