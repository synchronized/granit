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
  -DGRANIT_FETCH_INTEGRATION_DEPENDENCIES=ON
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

查看器支持右键环绕、中键平移、滚轮缩放、`F` 聚焦选择和 `Home` 恢复视图。窗口标题显示实际
后端与 Adapter；Renderer、场景、材质、灯光和性能信息位于 ImGui 面板。

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
