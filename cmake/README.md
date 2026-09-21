# CMake 构建目录

本目录承载 Granit 的构建配置，按职责分层。约定：只有主构建模块与安装入口放在顶层，
独立脚本归入子目录，测试校验脚本归入 `tests/cmake/`。

## 顶层：主构建模块

由根 `CMakeLists.txt` 通过 `include()` 加载，定义选项、平台探测、通用函数与目标。

| 文件 | 角色 |
| --- | --- |
| `granit_version.cmake` | 工程版本唯一来源，并派生 major、minor、patch 与兼容版本 |
| `granit_options.cmake` | 构建选项（`BUILD_SHARED_LIBS`、`GRANIT_BUILD_*` 等） |
| `granit_platform.cmake` | 平台探测（Emscripten / XCB / Wayland 能力） |
| `granit_features.cmake` | 派生开关（`GRANIT_TESTING_ENABLED`、`GRANIT_NEEDS_*`） |
| `granit_utility.cmake` | 通用函数（输出目录、编译警告、枚举校验） |
| `granit_asset_paths.cmake` | 资产路径 |
| `granit_integration_dependencies.cmake` | 可选集成依赖准备与依赖获取策略（SDL3 / ImGui） |
| `granit_web.cmake` | WebGPU / Emscripten 辅助函数 |
| `granit_shader_assets.cmake` | Shader 资产构建 |
| `granit_build_assets.cmake` | 资产构建入口 |
| `granit_modules.cmake` | 内部模块定义（math / material / pbr / scene / lighting） |
| `granit_render_pipeline_module.cmake` | RenderPipeline 模块 |
| `granit_package.cmake` | 打包 / 安装配置 |
| `granitConfig.cmake.in` | 包配置模板（生成 `granitConfig.cmake`） |

## `toolchain/`：Shader 工具链子系统

| 文件 | 角色 |
| --- | --- |
| `granit_shader_toolchain.cmake` | 工具链主模块（查找 / 下载 / 校验 DXC 与 Tint） |
| `GranitShaderToolchain.cmake` | 安装给下游的大小写稳定入口 |
| `GranitShaderToolchainLock.cmake` | 工具链锁定身份（版本 / 下载地址 / SHA256） |
| `download_shader_toolchain.cmake` | 下载工具链 |
| `generate_shader_toolchain_manifest.cmake` | 生成 manifest |
| `verify_shader_toolchain_manifest.cmake` | 校验 manifest |
| `package_shader_toolchain.cmake` | 打包工具链 |

## `assets/`：独立资产脚本

由 `cmake -P` 独立调用，不在主构建图内。

| 文件 | 角色 |
| --- | --- |
| `fetch_flight_helmet.cmake` | 下载飞行头盔示例模型 |
| `fetch_model_viewer_environment.cmake` | 下载模型查看器环境 |
| `embed_binary.cmake` | 二进制文件嵌入 C 数组 |
| `collect_license_bundle.cmake` | 收集第三方许可证 |

## 测试校验脚本

13 个 `check_*.cmake` 位于 `tests/cmake/`，由 CTest（`tests/*/CMakeLists.txt`）与 CI
（`.github/workflows/*.yml`）通过 `cmake -P` 调用，验证文档、安装导出、Shader 工具链等。

## 安装

`toolchain/` 下由 `granit_package.cmake` 引用的文件会**平铺安装**到
`lib/cmake/granit/`，下游 `find_package(granit COMPONENTS AssetTools)` 后按安装目录
内的相对路径互相引用。
