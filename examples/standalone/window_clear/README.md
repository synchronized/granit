<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 安装 SDK Window Quickstart

这个独立工程只使用 Granit 安装包导出的公共头文件和 `granit::window` CMake 目标。它创建 Window、
Renderer、Surface、Swapchain 与 Frame Context，持续清屏并呈现，适合作为外部 C++20 项目的起点。

先安装或解压 Granit shared SDK，再单独配置本目录：

```powershell
cmake -S examples/standalone/window_clear -B build/window-clear `
  -DCMAKE_PREFIX_PATH=C:/path/to/granit
cmake --build build/window-clear --config Release
ctest --test-dir build/window-clear -C Release --output-on-failure
```

直接运行 `granit_window_clear` 会保持窗口直到关闭；CTest 使用 `--smoke-test` 呈现三帧后退出。无可用
GPU、驱动或显示环境时 Smoke 返回跳过状态，其他初始化和生命周期错误仍会失败。

本工程不能依赖 `examples/common`、源码树 CMake helper 或内部头文件。仓库内的完整教程与综合应用
分别位于 `examples/tutorials` 和 `examples/samples`。
