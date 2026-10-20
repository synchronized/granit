<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 第三方依赖与缓存

本文是 Granit 第三方依赖获取、目标复用和源码缓存布局的权威说明。许可证和来源信息仍以各依赖
目录下的 `THIRD_PARTY_NOTICES.md` 为准。

## 依赖分类

| 依赖 | 用途 | 作用范围 | 获取方式 |
|---|---|---|---|
| SDL3 | Window、输入和 SDL Surface | Window/示例 | 系统 target 或锁定源码 |
| ImGui | ImGui Draw Data 和示例 UI | 可选 Integration/示例 | 系统 target 或锁定源码 |
| cgltf | glTF/GLB 解析 | 示例 Model Viewer | 系统 target 或锁定源码 |
| stb_image | PNG/JPEG 解码 | 示例 Model Viewer | 系统 target 或锁定源码 |
| Unity、Catch2 | 测试框架 | 测试目标 | 系统包或 `vendor/cache` 锁定源码 |
| Shader Toolchain | DXC、Tint 等离线工具 | Shader 资产构建 | 独立工具链缓存 |
| Vulkan-Headers、Volk、VMA | Vulkan 内部实现 | Granit 核心 | 仓库内置 `3rd/` |

SDL3、ImGui、cgltf 和 stb_image 不进入 Granit 公共头文件；cgltf 和 stb_image 也不进入安装 SDK。

## 获取策略

下载型依赖使用全局策略和单项覆盖：

```text
GRANIT_DEPENDENCY_POLICY
  ├─ GRANIT_DEPENDENCY_SDL3
  ├─ GRANIT_DEPENDENCY_IMGUI
  ├─ GRANIT_DEPENDENCY_CGLTF
  └─ GRANIT_DEPENDENCY_STB
```

每项覆盖为空时继承全局策略。三种策略的行为如下：

| 策略 | 行为 |
|---|---|
| `system` | 只查找父项目 target 或 `find_package`；缺失时配置失败 |
| `auto` | 先查找父项目 target 或系统包，缺失时使用锁定源码缓存 |
| `download` | 跳过父项目和系统包，强制使用锁定版本源码缓存 |

因此，父项目已经提供兼容 target 时，`auto` 不会重复下载或覆盖该 target。需要强制统一版本时才
使用 `download`。

## 父项目 target 优先级

集成模块在 `system` 和 `auto` 策略下优先复用已有 target。例如：

- SDL3：`SDL3::SDL3`；
- ImGui：`imgui::imgui`、`ImGui::ImGui` 或 `imgui`；
- cgltf：`cgltf::cgltf` 或 `cgltf`；
- stb：`stb::image` 或 `Stb::Stb`。

这些 target 必须由父项目在 Granit 配置对应模块之前提供。`download` 策略会跳过这些查找，这是为了
保证可复现的锁定版本构建。

## 源码缓存

下载型源码统一使用 `GRANIT_DEPENDENCY_SOURCE_CACHE_DIR`，默认值为 `vendor/cache`：

```text
vendor/cache/
├─ locks/<name>-<commit>.lock
├─ archives/<name>-<commit>.tar.gz
├─ sources/<name>/<commit>/
└─ staging/<name>-<commit>-<nonce>/ # 下载和解压临时目录，成功后删除
```

源码依赖的版本、commit、下载地址、SHA-256 和 marker 文件唯一维护在
[`cmake/granit_dependency_catalog.cmake`](../../cmake/granit_dependency_catalog.cmake)。缓存流程由
[`cmake/granit_dependency_cache.cmake`](../../cmake/granit_dependency_cache.cmake) 负责。

缓存只保存可校验、可重新生成的下载内容，不提交第三方源码，也不进入安装包。不同平台、Debug、
Release、静态和共享 preset 复用源码，但各自保留自己的编译产物和 CMake 生成目录。

每个“依赖名 + commit”使用独立的 CMake 进程锁，等待上限为 300 秒。持锁期间只写入随机命名的
`staging` 目录；下载完成后校验 SHA-256，解压并检查 marker，再写入 `.granit-source-complete`
完成收据，最后以目录重命名的方式发布到 `sources`。因此并行 configure 不会读到正在下载、解压或
校验中的半成品；已有缓存若缺少完成收据，会在首次复用时补齐收据。

删除对应的 `archives` 或 `sources` 子目录即可强制重新获取；修改锁定 commit 后会使用新的缓存路径。

## 依赖 target 的组装方式

缓存流程统一，但 target 创建方式按依赖类型保持不同：

- SDL3：使用缓存源码的 CMake 项目，通过 `FetchContent` 创建 SDL target；
- ImGui：从缓存源码选取 Granit 所需源文件，创建 Granit 私有静态目标；
- cgltf/stb_image：创建只暴露 include directory 的 `INTERFACE` 目标；
- 测试框架：优先复用父项目或系统包，缺失时使用锁定源码缓存构建 CMake 目标；内部实现目标使用
  `granit_test_*` 名称，兼容 alias 仅在外部目标不存在时创建；
- Shader Toolchain：使用独立的二进制归档、manifest 和能力检查，不混入源码缓存。

因此，统一的是“身份、校验、缓存和策略”，不是把所有第三方库强行转换成同一种 CMake target。

## 测试框架与父项目共存

Unity 和 Catch2 只参与 Granit 的测试构建，不进入安装导出目标，也不成为 Granit 公共库的传递
依赖。测试配置会先复用父项目已经提供的兼容 target，避免在同一个构建树中重复编译测试框架。

Catch2 要求 3.x；如果已发现的包明确报告低于 3.0 的版本，配置会直接失败，而不会静默混用
Catch2 2.x。缓存源码 fallback 使用 `granit_test_*` 私有目标，只有为了兼容现有测试 CMake
文件，且在同名外部 target 不存在时才创建 `Catch2::` 或 `Unity::` alias。

将测试框架放入 `vendor/cache` 不能解决 target 名称或版本冲突；Granit 仍通过 `granit_test_*`
目标隔离内部实现，并按策略复用父项目目标。`vendor/cache` 负责其锁定源码的下载、校验和并发
填充，测试框架源码本身不提交到仓库。

## 离线和系统包构建

只使用系统包并让缺失依赖立即失败：

```sh
cmake -S . -B build/system \
  -DGRANIT_DEPENDENCY_POLICY=system \
  -DGRANIT_SHADER_TOOLCHAIN_MODE=system
```

使用仓库已填充缓存的可复现构建：

```sh
cmake -S . -B build/offline \
  -DGRANIT_DEPENDENCY_POLICY=download \
  -DGRANIT_DEPENDENCY_SOURCE_CACHE_DIR=/path/to/vendor/cache
```

若缓存缺失，`download` 配置会在获取阶段失败；不会静默回退到其他版本。
如需显式禁止网络访问，可额外设置 `-DGRANIT_DEPENDENCY_OFFLINE=ON`；缓存缺失时会立即失败。

## 权威来源

- 构建选项和 preset：[`构建与安装`](build.md)；
- 版本和校验信息：[`cmake/granit_dependency_catalog.cmake`](../../cmake/granit_dependency_catalog.cmake)；
- 许可证和上游来源：各依赖目录的 `THIRD_PARTY_NOTICES.md`；
- Shader Toolchain 锁定信息：[`GranitShaderToolchainLock.cmake`](../../cmake/toolchain/GranitShaderToolchainLock.cmake)。
