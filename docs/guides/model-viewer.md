<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行跨后端模型查看器

模型查看器使用同一套 CPU Scene、GPU Scene 和 Render Pipeline，通过 Vulkan 或桌面 Dawn
WebGPU 显示 glTF 2.0 模型，并叠加 ImGui 调试面板。它是仓库内示例，不属于 Granit 安装包，
其中的 glTF 加载器也不是公共 SDK。

## 构建桌面查看器

桌面目标依赖 SDL3 和 ImGui。当前 CMake 只在显式启用模型查看器、两个 Integration，并允许获取
锁定集成依赖时生成 `granit_model_viewer_example`：

```powershell
cmake -S . -B build/model-viewer -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DGRANIT_BUILD_MODEL_VIEWER_EXAMPLE=ON `
  -DGRANIT_BUILD_INTEGRATION_SDL3=ON `
  -DGRANIT_BUILD_INTEGRATION_IMGUI=ON `
  -DGRANIT_FETCH_INTEGRATION_DEPENDENCIES=ON `
  -DGRANIT_FETCH_EXAMPLE_GLTF_DEPENDENCIES=ON
cmake --build build/model-viewer --target granit_model_viewer_example
```

默认构建包含 Vulkan 后端。若还要使用桌面 Dawn WebGPU，应额外提供与当前平台和工具链匹配的
锁定 Dawn SDK：

```powershell
cmake -S . -B build/model-viewer-webgpu -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DGRANIT_BUILD_MODEL_VIEWER_EXAMPLE=ON `
  -DGRANIT_BUILD_INTEGRATION_SDL3=ON `
  -DGRANIT_BUILD_INTEGRATION_IMGUI=ON `
  -DGRANIT_FETCH_INTEGRATION_DEPENDENCIES=ON `
  -DGRANIT_FETCH_EXAMPLE_GLTF_DEPENDENCIES=ON `
  -DGRANIT_BUILD_BACKEND_WEBGPU=ON `
  -DGRANIT_DAWN_ROOT=D:/path/to/dawn-sdk
cmake --build build/model-viewer-webgpu --target granit_model_viewer_example
```

Dawn SDK 的获取、工具链兼容性和插件位置见[构建与安装](build.md#实验性-webgpu-插件)。

## 获取验收模型

仓库不提交完整 FlightHelmet。以下命令按 manifest 下载锁定版本并逐文件校验 SHA-256：

```powershell
cmake `
  -DGRANIT_SOURCE_DIR="$PWD" `
  -DGRANIT_FLIGHT_HELMET_OUTPUT_DIR="$PWD/build/assets/FlightHelmet" `
  -P cmake/fetch_flight_helmet.cmake
```

资产来自 Khronos glTF Sample Assets，模型使用 CC0-1.0。锁定版本和第三方通知见
[`FlightHelmet.manifest.json`](../../examples/assets/FlightHelmet.manifest.json) 与
[`THIRD_PARTY_NOTICES.md`](../../examples/common/gltf/THIRD_PARTY_NOTICES.md)。

## 运行

使用 Vulkan：

```powershell
build/model-viewer/bin/granit_model_viewer_example.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --backend=vulkan --validation
```

使用桌面 Dawn WebGPU：

```powershell
build/model-viewer-webgpu/bin/granit_model_viewer_example.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --backend=webgpu `
  --backend-library build/model-viewer-webgpu/bin/granit_backend_webgpu.dll
```

Linux 的插件文件通常为 `libgranit_backend_webgpu.so`。省略 `--backend-library` 时，Registry 按
默认插件搜索规则加载后端；开发构建推荐传入明确路径，避免误用系统中的旧插件。

可用参数如下：

| 参数 | 作用 |
| --- | --- |
| `--asset <文件>` | 必填；指定 `.gltf` 或 `.glb` 主文件 |
| `--backend=auto\|vulkan\|webgpu` | 选择 Renderer 后端 |
| `--backend-library <文件>` | 指定后端插件动态库 |
| `--validation` | 启用可用的后端验证层 |
| `--smoke-test` | 渲染少量帧后自动退出 |
| `--no-ui` | 不创建或绘制 ImGui 资源，用于测量纯场景渲染 |
| `--present-mode=fifo\|immediate` | 指定呈现模式；不可用时性能采样会明确失败 |
| `--profile-output <文件.json>` | 固定运行 300 帧预热和 1000 帧采样并写出性能报告 |

查看器支持右键环绕、中键平移、滚轮缩放、`F` 聚焦选择和 `Home` 恢复视图。窗口标题显示实际
后端与 Adapter；Renderer、场景、材质、灯光和性能信息位于 ImGui 面板。

`--smoke-test` 与 `--profile-output` 用途不同，不能同时使用。

## 采集 Release 性能基线

性能模式固定窗口像素尺寸为 1920×1080，并使用当前模型查看器的固定初始相机。正式数据应使用
Release、关闭 Validation，并分别采集 UI 开/关及 Immediate/FIFO。以下命令采集 Vulkan、
Immediate、无 UI 的基线：

```powershell
build/model-viewer/bin/granit_model_viewer_example.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --backend=vulkan --present-mode=immediate --no-ui `
  --profile-output build/results/vulkan-immediate-no-ui.json
```

删除 `--no-ui` 可测量完整 ImGui 路径，将呈现模式改为 `fifo` 可测量垂直同步路径。桌面 Dawn 使用
相同参数，只需按前文切换后端和插件路径。若驱动不支持请求的呈现模式或窗口像素尺寸不是
1920×1080，程序会失败而不会把回退结果混入基线。

JSON 记录资产、实际后端、Adapter、呈现模式、UI 和 Validation 状态，并分别报告 CPU 帧时间、
帧槽等待、Present 等待与 GPU 时间戳的 p50/p95/p99 和有效样本数。GPU Timestamp 不可用时对应
样本数为零；各项时间不得相加解释为总帧时间。首份结果用于建立可复现基线，不作为跨硬件 FPS
门槛。

## 生成固定验收截图

原生构建还会生成无窗口验收程序。它使用固定相机和 512×512 RGBA8 输出，预热三帧后将紧密
原始像素写入文件：

```powershell
build/model-viewer/bin/granit_model_viewer_offscreen_acceptance.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --output build/acceptance/flight-helmet-vulkan.rgba `
  --backend=vulkan --validation
```

传入 `--expected` 时，程序会按轮廓边缘容差、非边缘颜色 MAE 和异常像素比例比较期望图。失败时
保留实际图，并在同一目录生成 `<输出名>.diff.rgba` 和 `<输出名>.report.json`；报告包含实际后端、
Adapter、资产路径及量化统计，供 Actions 一并上传。桌面 Dawn 使用相同程序，只需切换后端与
插件路径：

```powershell
build/model-viewer-webgpu/bin/granit_model_viewer_offscreen_acceptance.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --output build/acceptance/flight-helmet-webgpu.rgba `
  --expected build/acceptance/flight-helmet-vulkan.rgba `
  --backend=webgpu `
  --backend-library build/model-viewer-webgpu/bin/granit_backend_webgpu.dll
```

`.rgba` 文件固定为 1,048,576 字节，不含行填充或文件头。当前阶段以 Vulkan 基准比较 Dawn；
基准更新必须随 Renderer、Adapter、模型 manifest 和变更原因一起评审。

维护者可手动运行 `Dawn Integration` Actions。工作流先在 Linux Lavapipe 生成并校验 Vulkan
参考图，再让 Windows Dawn D3D12 和 Linux Dawn Vulkan 下载同一参考图执行分层比较。成功时只保留
三天的跨 Job 参考图；失败时额外上传实际图、差异图、JSON 报告、运行日志和 FlightHelmet
manifest，保留七天供定位。该工作流不会由提交或合并自动触发。

## 浏览器验证

Emscripten 版本目前是自动化 Fixture，而不是面向用户发布的完整网页查看器。它通过同一个
Application Core 验证模型 Fetch、PBR 绘制、60 帧循环、输入、Resize、错误资产诊断和退出时
资源归零。构建与运行测试：

```powershell
cmake --preset emscripten-release
cmake --build --preset emscripten-release
cd web/tests
npm ci
$env:CHROME_PATH = "C:\Program Files\Google\Chrome\Application\chrome.exe"
npm test -- ../../build/emscripten-release/web
```

## 常见问题

- 没有生成桌面可执行文件：确认模型查看器、SDL3、ImGui 和依赖获取四个选项均已启用。
- WebGPU 后端不可用：确认 Dawn SDK 与平台、架构、编译器和运行库匹配，并显式传入插件路径。
- 模型加载失败：保持 `.gltf` 与其 `.bin`、纹理的相对目录结构；当前加载器拒绝绝对 URI、父目录
  跳转、网络 URI 和不支持的 glTF 扩展。
- 页面不能直接打开：浏览器产物必须通过 HTTP 服务访问；自动化测试会自行启动本地服务器。
