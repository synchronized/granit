<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 运行跨后端模型查看器

模型查看器使用同一套 CPU Scene、GPU Scene 和 Render Pipeline，在桌面 Vulkan 与浏览器
Emscripten WebGPU 上显示 glTF 2.0 模型。桌面目标叠加 ImGui 调试面板；该示例及其 glTF 加载器
不属于 Granit 安装 SDK。

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

## 获取验收模型

仓库不提交完整 FlightHelmet。以下命令按 manifest 下载锁定版本并逐文件校验 SHA-256：

```powershell
cmake `
  -DGRANIT_SOURCE_DIR="$PWD" `
  -DGRANIT_FLIGHT_HELMET_OUTPUT_DIR="$PWD/build/assets/FlightHelmet" `
  -P cmake/fetch_flight_helmet.cmake
```

环境光源也采用独立清单下载，不进入默认 Git 工作树：

```powershell
cmake `
  -DGRANIT_MODEL_VIEWER_ENVIRONMENT_OUTPUT_DIR="$PWD/build/assets/StudioSmall03" `
  -P cmake/fetch_model_viewer_environment.cmake
```

该命令获取 Poly Haven 以 CC0-1.0 发布的 `Studio Small 03` 1K HDR，并同时校验文件大小和
SHA-256。来源和固定校验值见
[`StudioSmall03.manifest.json`](../../examples/assets/StudioSmall03.manifest.json)。当前文件是后续
离线预处理的输入，不是 model-viewer 可直接加载的运行时环境包。

使用 glTF IBL Sampler 生成未压缩 RGBA16F 输入后，通过仓库工具打包：

```powershell
<ibl-sampler> -inputPath studio_small_03_1k.hdr -outCubeMap diffuse.ktx2 `
  -distribution Lambertian -sampleCount 1024 -cubeMapResolution 32 `
  -targetFormat R16G16B16A16_SFLOAT
<ibl-sampler> -inputPath studio_small_03_1k.hdr -outCubeMap specular.ktx2 `
  -outLUT brdf_lut.png -distribution GGX -sampleCount 1024 -cubeMapResolution 128 `
  -mipLevelCount 8 -targetFormat R16G16B16A16_SFLOAT
build/windows-clang-debug/bin/granit_model_viewer_environment_tool.exe build `
  --irradiance diffuse.ktx2 --prefiltered specular.ktx2 `
  --brdf-lut brdf_lut.png --output StudioSmall03.grenv `
  --intensity 0.12 --exposure -0.5
```

打包工具只接受未压缩 RGBA16F 六面 KTX2；GGX 输入必须包含一直到 1×1 的完整 Mip 链。
BRDF LUT PNG 会被确定性转换为 RGBA16F。`--intensity` 和 `--exposure` 写入该环境推荐的初始
光照参数；查看器加载后采用这些值，用户仍可在 Lighting 面板继续调整。工具不属于安装 SDK，
也不会成为应用运行时依赖。
仓库已提交同一参数生成的 `examples/assets/StudioSmall03.grenv`；其大小、SHA-256、采样参数和
glTF IBL Sampler 修订号记录在 manifest 中。

查看器省略 `--environment` 时会创建一个低分辨率内置摄影棚环境，保证离线首次运行时金属与暗部
仍然可读；传入 GRENV v2 则使用预处理的高质量环境及其推荐光照参数进行跨后端图像验收。
查看器默认使用方向光作为主光，并以低强度环境光补充暗部和金属反射；两者均可在 Lighting 面板调整。

桌面查看器会在后台读取和解析 glTF、解码纹理，并在主线程持续显示阶段进度和处理窗口事件。
GPU 资源上传仍在主线程执行，但会按纹理分批提交，并在 Geometry、Texture、Sampler、Mesh 和
Material 资源边界刷新进度与处理窗口事件。`--no-ui` 模式保持同步等待行为。

资产来自 Khronos glTF Sample Assets，模型使用 CC0-1.0。锁定版本和第三方通知见
[`FlightHelmet.manifest.json`](../../examples/assets/FlightHelmet.manifest.json) 与
[`THIRD_PARTY_NOTICES.md`](../../examples/common/gltf/THIRD_PARTY_NOTICES.md)。

## 运行

使用 Vulkan：

```powershell
build/model-viewer/bin/granit_model_viewer_example.exe `
  --asset build/assets/FlightHelmet/glTF/FlightHelmet.gltf `
  --environment examples/assets/StudioSmall03.grenv `
  --backend=vulkan --validation
```

可用参数如下：

| 参数 | 作用 |
| --- | --- |
| `--asset <文件>` | 必填；指定 `.gltf` 或 `.glb` 主文件 |
| `--environment <文件>` | 可选；指定 GRENV v2 环境包，省略时使用内置低分辨率环境 |
| `--backend=auto\|vulkan` | 选择桌面 Renderer 后端 |
| `--validation` | 启用可用的后端验证层 |
| `--smoke-test` | 渲染少量帧后自动退出 |
| `--no-ui` | 不创建或绘制 ImGui 资源，用于测量纯场景渲染 |
| `--present-mode=fifo\|mailbox\|immediate` | 指定呈现模式；默认 Mailbox，不可用时后端可回退 FIFO；性能采样会拒绝回退 |
| `--profile-output <文件.json>` | 固定运行 300 帧预热和 1000 帧采样并写出性能报告 |

查看器支持右键环绕、中键平移、滚轮缩放、`F` 聚焦选择和 `Home` 恢复视图。窗口标题显示实际
后端与 Adapter；Renderer、场景、材质、灯光和性能信息位于 ImGui 面板。Renderer 面板允许在
运行时切换 1×/4× MSAA、FXAA、Specular AA 和 1×～16× 各向异性；超过设备上限的倍率不会
生效。管线选项会事务式创建新 Render Pipeline，各向异性变化会事务式重建 GPU Scene 中的
Sampler 和材质绑定；创建失败时保留原配置。

`--smoke-test` 与 `--profile-output` 用途不同，不能同时使用。

离屏验收程序 `granit_model_viewer_offscreen_acceptance` 额外接受 `--msaa=1|4`、
`--fxaa=on|off`、`--specular-aa=on|off` 和 `--anisotropy=1|2|4|8|16`。它会通过公开的
Renderer Limits 严格校验请求，不支持的配置直接失败而不会静默回退。桌面 Vulkan 与浏览器
WebGPU 使用相同的公共配置语义。

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

删除 `--no-ui` 可测量完整 ImGui 路径，将呈现模式改为 `fifo` 可测量垂直同步路径。若驱动不支持
请求的呈现模式或窗口像素尺寸不是 1920×1080，程序会失败而不会把回退结果混入基线。

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
  --environment examples/assets/StudioSmall03.grenv `
  --output build/acceptance/flight-helmet-vulkan.rgba `
  --backend=vulkan --validation
```

传入 `--expected` 时，程序会按一像素轮廓边缘容差、最多四个孤立轮廓残差、非边缘颜色 MAE
和异常像素比例比较期望图。该孤立像素上限用于吸收不同 Rasterizer 的边界覆盖规则，不允许
整段轮廓偏移。失败时
保留实际图，并在同一目录生成 `<输出名>.diff.rgba` 和 `<输出名>.report.json`；报告包含实际后端、
Adapter、资产路径及量化统计，供 Actions 一并上传。`.rgba` 文件固定为 1,048,576 字节，不含
行填充或文件头；基准更新必须随 Renderer、Adapter、模型 manifest 和变更原因一起评审。

## 浏览器验证

Emscripten 版本目前是自动化 Fixture，而不是面向用户发布的完整网页查看器。它通过同一个
Application Core 验证模型 Fetch、PBR 绘制、60 帧循环、输入、Resize、错误资产诊断和退出时
资源归零。Fixture 的 glTF 是用于确定性测试的三角模型，因此页面显示三角形并不表示回退到了
旧的独立三角示例。自动化测试还会依次切换 1×/全关闭与 4×/FXAA/Specular AA 配置，并按设备
上限验证各向异性重建。构建与运行测试：

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
- 浏览器 WebGPU 不可用：确认使用锁定 emsdk 构建，并检查浏览器 WebGPU 支持与控制台诊断。
- 模型加载失败：保持 `.gltf` 与其 `.bin`、纹理的相对目录结构；当前加载器拒绝绝对 URI、父目录
  跳转、网络 URI 和不支持的 glTF 扩展。
- 页面不能直接打开：浏览器产物必须通过 HTTP 服务访问；自动化测试会自行启动本地服务器。
