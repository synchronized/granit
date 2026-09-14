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

同一份 ImGui 内容也可通过 SDL3 和浏览器 WebGPU 运行。先激活 Emscripten 环境，再执行：

```powershell
$env:EMSDK = "D:/sunday/programs/emsdk"
cmake --preset emscripten-release
cmake --build --preset emscripten-release --target granit_imgui_web
python -m http.server 8000 --directory build/emscripten-release/web
```

随后使用支持 WebGPU 的浏览器打开
`http://localhost:8000/granit_imgui_web.html`。页面使用 SDL3 处理浏览器事件，ImGui 仍通过 Granit
Canvas 绘制，不是 DOM/CSS 仿制界面，因此可用于核对桌面与 Web 的字体、纹理、裁剪和输入一致性。

## Model Viewer

`granit_model_viewer_example` 是跨后端的完整渲染示例。它复用同一应用核心，在桌面 Vulkan 和
浏览器 Emscripten WebGPU 中加载 glTF/GLB、上传 GPU Scene 并显示 PBR 模型；桌面目标还提供
编辑器式面板。该目标需要显式启用模型查看器及对应 Integration。

构建、资产获取、命令行参数和排错见[跨后端模型查看器指南](model-viewer.md)。

## ImGui 固定画面验收

启用示例、SDL3/ImGui 集成和测试后，可运行 Vulkan 离屏视觉验收：

```powershell
cmake --build --preset windows-clang-debug --target granit_imgui_visual_test
ctest --preset windows-clang-debug -R "^granit\.example\.imgui_visual$" --output-on-failure
```

该测试在 1×、2× 帧缓冲比例下渲染固定文字、四分区纹理、裁剪矩形和点击状态，复用截图比较器
检查颜色区域，并将 PPM 产物写入构建目录的 `examples/samples/imgui`。它验证 Vulkan 渲染与
ImGui 输入队列，不替代 SDL3 窗口的真实显示缩放和焦点验收。

浏览器测试使用同一画面，通过真实鼠标点击验证内容状态及像素变化：

```powershell
node tests/web/imgui_test.cjs build/emscripten-release/web
```

先按[浏览器指南](webgpu-browser-example.md)安装测试驱动并设置 `CHROME_PATH`。测试分别创建
1×、2× DPI 浏览器上下文，PNG 保存到 `build/emscripten-release/web/validation`。手动查看固定画面
可打开 `granit_imgui_web.html?validation=1`。稳定基准采用固定区域的颜色容差、字体覆盖、裁剪和
纹理差异断言，整幅截图仅作为诊断产物。整图会受字体栅格和驱动差异影响，因此不作为跨平台的
逐像素通过条件。

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
assets/sources/shaders/pipeline/   正式 Pipeline 内置 Shader
assets/sources/shaders/pbr/     示例、测试和工具共享的 PBR 参考 Shader
examples/assets/        Model Viewer 的示例资产
tests/fixtures/         测试与 Smoke 固定输入
```

正式库源码不得反向依赖 `examples` 或 `tests`。公开示例不包含 Vulkan 头文件；预编译 Shader
随仓库提供，普通构建不要求运行时 Shader 编译器。

示例源码按职责分层：`examples/common` 按技术域保存多个示例共用的平台辅助代码，
`examples/samples` 保存各示例内容和自身目标声明。两者均为仓库私有实现，不随 SDK 安装。
