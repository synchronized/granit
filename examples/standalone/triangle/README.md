<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 安装 SDK Triangle

该独立工程只使用安装后的 Granit CMake package。它在 Window 清屏闭环上增加 Shader Library、
Graphics Pipeline 和 `draw(3)`，适合作为安装 SDK 的首个图形程序。

默认直接使用随示例提交的 `triangle.grshlib`，应用运行时不需要 DXC 或 Tint：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/granit-sdk
cmake --build build
```

安装包同时包含 AssetTools 和 `granit_asset_tool` 时，可以用同一份 HLSL 与清单重建跨 Vulkan、
WebGPU 的 Shader Library。缺少工具链时，Granit CMake 模块会下载并校验锁定工具链包：

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/granit-sdk \
  -DGRANIT_TRIANGLE_REBUILD_SHADERS=ON
cmake --build build
```

应用通过 Library 内的 `triangle.vertex` 与 `triangle.fragment` 逻辑名称创建 Shader，不依赖生成的
内容摘要或后端文件名。
