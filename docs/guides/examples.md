<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 示例程序

Granit 只保留两个面向使用者的示例系列。它们用于展示完整应用集成；版本查询、离屏清屏、
纹理回读和平台窗口等单能力验证位于 `tests/smoke`，由构建与 CTest 覆盖，不再作为示例发布。

## SDL3 + ImGui

`granit_sdl3_imgui_example` 展示 SDL3 窗口、输入、ImGui Platform Backend、Canvas 转换、
纹理、裁剪、Resize 与 Present 的完整集成。目标仅在启用 SDL3、ImGui Integration 和锁定依赖时
生成。

```powershell
cmake --preset windows-clang-release
cmake --build --preset windows-clang-release --target granit_sdl3_imgui_example
build/windows-clang-release/bin/granit_sdl3_imgui_example.exe
```

可使用 `--frames-in-flight 1..4` 选择帧槽数，使用
`--present-mode immediate|fifo` 选择呈现模式。性能采样示例：

```powershell
build/windows-clang-release/bin/granit_sdl3_imgui_example.exe `
  --no-validation --present-mode immediate --frames-in-flight 3 `
  --profile-warmup 120 --profile-frames 600 --profile-output imgui-profile.csv
```

CSV 记录窗口尺寸、帧槽、Validation、Present Mode、CPU 阶段和 GPU Timestamp；退出前尚未回收
的样本保持空值，不按零处理。

## Model Viewer

`granit_model_viewer_example` 是跨后端的完整渲染示例。它复用同一应用核心，在桌面 Vulkan 和
浏览器 Emscripten WebGPU 中加载 glTF/GLB、上传 GPU Scene 并显示 PBR 模型；桌面目标还提供
编辑器式面板。该目标需要显式启用模型查看器及对应 Integration。

构建、资产获取、命令行参数和排错见[跨后端模型查看器指南](model-viewer.md)。

## 内部 Smoke 程序

`tests/smoke` 保存最小 GPU、Render Pipeline 与平台集成程序。它们使用 `_smoke` 目标后缀和
`granit.smoke.*` CTest 名称，服务于回归、CI 和底层排错，不承诺示例级交互体验，也不作为安装
内容。需要执行现有自动 Smoke 时使用：

```powershell
ctest --preset windows-clang-debug -R "^granit\.smoke\."
```

其中纹理回读测试仍可直接运行并传入 `.rgba` 输出路径，详见
[纹理同步回读](texture-readback.md)。Render Pipeline 的分步说明见
[离屏渲染教程](../tutorials/render-pipeline-offscreen.md)。

## 资源归属

```text
src/pipeline/shaders/   正式 Pipeline 内置 Shader
assets/shaders/pbr/     示例、测试和工具共享的 PBR 参考 Shader
examples/assets/        Model Viewer 的示例资产
tests/fixtures/         测试与 Smoke 固定输入
```

正式库源码不得反向依赖 `examples` 或 `tests`。公开示例不包含 Vulkan 头文件；预编译 Shader
随仓库提供，普通构建不要求运行时 Shader 编译器。
