<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.6 迁移到 0.7

## 适用场景

本文说明 Granit 0.6.0 Consumer 升级到 0.7.0 时需要处理的构建变化。0.7.0 聚焦 SDK component
边界和安装包选包语义，不改变公共 C ABI、C++ API、Shader 资产或材质包格式。

## 更新 CMake 请求版本

0.x 次版本允许有记录的破坏性变化，因此 0.7.0 不再接受旧次版本请求。将显式版本从 0.6 更新为
0.7：

```cmake
find_package(granit 0.7 CONFIG REQUIRED COMPONENTS RenderPipeline Window Input)
```

不带版本的 `find_package(granit CONFIG REQUIRED)` 仍会选择搜索路径中的任意 Granit Config 包，
适合由包管理器或固定 SDK 前缀保证版本的项目；需要防止误选时应始终声明 0.7。

## component 边界

- Core-only 应只链接 `granit::granit`，不会隐式导入其他 Granit component。
- `granit::input` 会导入其 `granit::window` 依赖。
- `granit::render_pipeline`、`granit::window` 和 `granit::input` 可以按需独立请求。
- `ShaderTools` 只存在于启用并安装该 component 的 SDK；缺失时 CMake 配置会明确失败。
- SDL3 与 ImGui Integration 仍要求调用方提供对应第三方依赖，不进入 Core。

## 重新构建

虽然 0.7.0 没有修改公共 C ABI，Granit 仍处于 0.x。升级时应清理旧构建目录，确保头文件、库、
可选 component 和运行时动态库全部来自同一套 0.7.0 SDK，然后重新编译应用。

## 验证

1. CMake 输出的 `granit_VERSION` 为 `0.7.0`。
2. 项目不再通过 `find_package(granit 0.6 ...)` 请求新 SDK。
3. Core-only 目标没有意外依赖 RenderPipeline、Window、Input 或 ShaderTools。
4. 共享库部署目录中没有混用 0.6.0 动态库。
