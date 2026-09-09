<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。项目当前仍处于 0.x；`Unreleased`
内容不代表已经发布。版本兼容规则见[版本与兼容策略](docs/reference/compatibility.md)。

## Unreleased

## 0.20.0 - 2026-09-10

### 新增

- 新增按 `examples/common` 与 `examples/samples` 分层的示例框架；Model Viewer 和 ImGui 提供
  Vulkan 桌面与 WebGPU 浏览器入口，并可独立构建和验收。
- ShaderTools 的 HLSL 编译描述支持末尾追加的预处理宏定义；宏集合进入确定性缓存身份。

### 变更

- 仓库 Shader 统一由构建过程生成 `.grshader` 清单及目标后端 sidecar；材质特性在生成阶段选择
  textured/untextured 等变体，运行时只加载打包结果。
- Model Viewer 在渲染背压期间保留输入增量并消费最新状态，桌面与浏览器共享相机、状态和加载
  语义；Web 面板补齐光照、进度、取消和诊断。
- Emscripten 分别验证平台 Smoke、正式 Model Viewer 和 ImGui，避免软件 WebGPU 外部实例在多个
  浏览器会话之间串扰。

### 修复

- 修复 WebGPU Canvas 的 Y 轴投影与左上原点裁剪不一致，以及 Canvas WGSL 未采样字体和自定义
  纹理导致文字方块、纹理纯白的问题；新增 ImGui 固定画面的 Vulkan/WebGPU 视觉回归。
- 开发 preset 现在会获取 Model Viewer 锁定的 glTF 私有依赖，修复全新构建目录无法直接配置的
  问题。

### 兼容性与迁移

- Frame 信息查询统一为 `granit_frame_get_info(renderer, frame, info)`，不再要求 Swapchain 句柄；
  删除 `granit_frame_get_slot_info` 导出。0.19 Consumer 必须改用新签名并重新编译；完整说明见
  [从 0.19 迁移到 0.20](docs/guides/migrate-0.19-to-0.20.md)。

## 0.19.0 - 2026-09-07

### 新增

- 新增版本化 Texture Asset Manifest、确定性编码和严格检查接口，稳定描述逻辑纹理、GPU 格式
  变体、内容 ID、负载摘要及逐 mip/数组层布局。
- 新增 Renderer 驱动的纹理变体选择，按清单顺序、设备格式能力、用途和 feature 要求选择首个
  兼容变体，缺少能力时明确返回 `UNSUPPORTED`。
- 新增逐 mip Texture Asset 写入 Upload Batch 的公共 C/C++ 接口，复用既有同步或异步提交、取消、
  背压及资源保活契约。

### 变更

- Manifest 检查拒绝未知版本、截断、尾部数据、整数溢出、重复或重叠子资源、不完整 mip 链及
  与格式块布局不一致的数据。
- Granit 只处理调用方持有的 Manifest 与已转码 GPU 负载；文件、网络、缓存、容器解析、运行时
  转码和最终资源切换仍由上游资产系统负责。

### 兼容性与迁移

- Core 新增四个 C ABI 导出和五个公共值结构。项目仍处于 0.x，Consumer 应重新编译并将 CMake
  请求版本更新为 0.19；Texture Asset Manifest v1 是新格式，不替代 KTX2、DDS 或图片容器。

## 0.18.0 - 2026-09-07

### 新增

- 新增 BC1、BC3、BC5、BC7、ETC2 RGBA8 与 ASTC 4×4 压缩纹理格式，并提供 UNORM/SRGB
  颜色空间变体。
- 新增格式块 Footprint、紧密数据布局计算和设备格式能力查询；上游可依据真实设备支持选择资产
  变体，无需复制 Granit 私有布局规则。
- Vulkan 与浏览器 WebGPU 的同步写入、Upload Batch、异步上传和 Command Recorder 复制统一支持
  压缩块布局、边缘 mip 与数组层跨度。

### 变更

- 开发工作流新增按改动范围选择的 `Quick Check`，并统一 Emscripten、Shader 工具链及正式打包的
  编译缓存。
- Release 改为不可变候选晋级：手动候选构建记录 tag、commit、run ID 与 SHA-256，正式标签只
  发布完全匹配的已验证产物，不再重复构建四套 SDK。
- Emscripten Chrome 验收显式覆盖软件 WebGPU 适配器的异步 Pipeline 已知失败与同步创建降级。

### 兼容性与迁移

- Core 新增格式枚举、能力结构和两个 C ABI 导出。项目仍处于 0.x，Consumer 应重新编译并将
  CMake 请求版本更新为 0.18。压缩纹理读回和运行时 Mipmap 生成暂不支持。

## 0.16.0 - 2026-09-06

### 变更

- Vulkan 异步 Upload/Readback Batch 在后端槽位饱和时立即返回可重试的 `NOT_READY`，不再等待
  调用线程；批次数据和资源保活状态保持完整，可在事件推进后重新提交。
- Vulkan Pipeline Warmup 在私有后台任务中执行冷创建，事件推进仅轮询完成状态，并据此报告
  `NON_BLOCKING_PIPELINE_WARMUP`；WebGPU 在具备原生异步创建链路前继续准确降级。
- 修正异步提交遇到背压时的临时操作回收和批次失败状态，避免操作泄漏或把暂时饱和误判为永久
  失败。

### 兼容性与迁移

- 本版本不新增 C ABI 符号和结构字段，只收紧既有异步 API 的非阻塞行为。调用方应把
  `NOT_READY` 视为背压并在推进 Renderer 事件后重试。完整说明见
  [从 0.15 迁移到 0.16](docs/guides/migrate-0.15-to-0.16.md)。

## 0.15.0 - 2026-09-06

### 新增

- Core 新增有界异步 Readback Batch，统一读取 Buffer 与 Texture 多区域，支持紧密或后端原始布局、
  结果元数据、显式复制和安全取消。
- Core 新增 Pipeline Warmup Batch、逐项结果、缓存命中信息及 32 字节稳定键；Vulkan 与 WebGPU
  通过同一 API 在加载阶段预热图形和计算 Pipeline。
- Renderer Limits 新增异步回读、Pipeline 预热和严格非阻塞预热能力位；调用方无需根据后端名称
  猜测行为。

### 变更

- Model Viewer 离屏验收和纹理回读 Smoke 改用异步 Readback，不再通过同步 Texture Readback 阻塞
  CPU。
- Renderer 资源统计追加 Readback/Pipeline Warmup Batch，取消后的可中止工作进入明确的
  `CANCELLED` 终态。

### 兼容性与迁移

- Core 新增 C ABI 导出、资源类型和 Renderer 能力位；资源统计结构尾部追加字段。0.x Consumer 应
  重新编译。完整步骤见[从 0.14 迁移到 0.15](docs/guides/migrate-0.14-to-0.15.md)。

## 0.14.0 - 2026-09-06

### 新增

- Upload Batch 新增异步提交入口和 move-only C++20 包装；调用方可通过统一异步操作查询完成、失败
  与取消请求状态。
- Upload Batch 描述新增最大暂存字节数和操作数，查询接口可读取当前占用与容量，用于在复制上传
  数据前实施背压。
- Renderer 资源统计追加公开异步操作数量，并将其计入 `total_live_count`，使关闭前泄漏检查覆盖
  Timestamp 和上传操作句柄。

### 变更

- 参考 Render Pipeline 的 GPU 指标改用 0.13.0 异步 Timestamp API，按帧槽轮询并只发布最新完成
  样本，不再同步读取 GPU 结果。
- Vulkan 异步上传通过 fence 非阻塞查询完成；WebGPU 在队列复制输入数据后按相同公共契约完成。
  提前销毁异步操作或目标资源仍由 Renderer 保留到 GPU 工作安全结束。
- Model Viewer 的几何与纹理批次改为异步提交；浏览器验收覆盖渲染、取消回滚、外部资源失败和
  关闭前资源归零。

### 兼容性与迁移

- Core 新增两个 C ABI 导出，并在 Upload Batch 描述尾部追加可选容量字段。0.x Consumer 应重新
  编译并将 CMake 请求版本更新为 0.14。完整步骤见
  [从 0.13 迁移到 0.14](docs/guides/migrate-0.13-to-0.14.md)。

## 0.13.0 - 2026-09-06

### 新增

- Core 新增后端无关的异步操作句柄、非阻塞状态查询、取消请求、结果查询和 move-only C++20
  RAII 包装。
- Timestamp Query 新增异步结果入口；Vulkan 与支持 `timestamp-query` 的浏览器 WebGPU 均通过
  相同公共契约返回 GPU 纳秒结果。
- 浏览器 Model Viewer 增加下载、解析、CPU 资产准备、GPU 上传和 Mipmap 的真实分阶段进度、
  取消及错误阶段报告。

### 变更

- 浏览器 WebGPU 仅在 Adapter 实际暴露且 Device 成功启用 `timestamp-query` 时报告对应能力；
  不支持设备稳定返回 `GRANIT_ERROR_UNSUPPORTED`。
- 浏览器加载阶段在 CPU/GPU 资源边界让出事件循环，加载期间页面可持续重绘并响应取消；失败或
  取消会回滚临时资源。
- Vulkan/WebGPU Smoke 统一验证异步状态、结果码、Timestamp 单调性、共享像素 Fixture 和关闭前
  资源归零。

### 兼容性与迁移

- Core 新增 C ABI 导出及异步操作资源类型，既有持久化资产格式不变。0.x Consumer 应重新编译并
  将 CMake 请求版本更新为 0.13。完整步骤见
  [从 0.12 迁移到 0.13](docs/guides/migrate-0.12-to-0.13.md)。

## 0.12.0 - 2026-09-06

### 新增

- Core 新增 `granit_shader_asset_inspect` 及 C++20 包装，无需创建 Renderer 即可校验 `.grshader`
  并取得内容 ID、缓存键、Stage、Entry Point 和后端变体摘要。
- RenderPipeline 新增公共标准 PBR Schema，稳定公开参数名、常量偏移、纹理特性、Binding、Vertex
  Location 和布局验证接口。
- RenderPipeline 安装标准 `materials/pbr_standard.grmat`，并公开模板版本与内容哈希。

### 变更

- Model Viewer 改为复用公共标准 PBR 材质，不再拥有示例私有模板。
- Package、FetchContent 与 `add_subdirectory` 统一提供 `granit_RENDER_PIPELINE_ASSET_DIR`，构建树
  Consumer 不再需要推导 Granit 源码目录。

### 兼容性与迁移

- Core 与 RenderPipeline 新增 C ABI 导出和公共头文件，既有函数语义及持久化格式不变。0.x
  Consumer 应重新编译并将 CMake 请求版本更新为 0.12。完整步骤见
  [从 0.11 迁移到 0.12](docs/guides/migrate-0.11-to-0.12.md)。

## 0.11.0 - 2026-09-06

### 新增

- 浏览器 WebGPU 补齐 Buffer/Texture 双向复制、Texture 复制、Buffer 填充和运行时 Mipmap；紧密
  行跨度由后端内部适配，不向公共 API 泄漏 256 字节编码约束。
- Emscripten 构建新增正式 `granit_model_viewer_web`，默认加载 Khronos Flight Helmet，支持通过
  URL 参数覆盖模型，并提供浏览器质量面板。
- Renderer Limits 新增 Timestamp Query 能力位和 C++ 便捷查询。

### 变更

- 浏览器 WebGPU 明确报告不支持现有同步 Timestamp Query 契约；Vulkan 报告支持。调用方不再需要
  通过后端名称猜测能力。
- 浏览器自动化扩展到真实传输、线性及 sRGB Mipmap、非二次幂、部分范围和 Cube 六面回归。

### 兼容性与迁移

- `granit_renderer_limits` 追加 `supported_features`。Granit 仍处于 0.x，Consumer 应重新编译并将
  CMake 请求版本更新为 0.11。完整步骤见[从 0.10 迁移到 0.11](docs/guides/migrate-0.10-to-0.11.md)。

## 0.10.0 - 2026-09-05

### 新增

- RenderPipeline 新增后端无关的 `granit_environment_map` C API 与 move-only C++20 RAII 包装，
  统一拥有 Irradiance Cube、Prefiltered Cube 和 BRDF LUT，并提供确定性内建降级环境。
- 正式发布 GRENV v3 环境资产格式，固定 RGBA16F 布局、推荐强度/曝光与 payload SHA-256；安装包
  在 `environments` 目录提供经过锁定的 Studio Small 03 资产及来源清单。
- Model Viewer 渲染执行器新增构造前容量提示和 `skipped_frame_builds` 统计。

### 变更

- Model Viewer 删除私有环境资源实现，桌面 Vulkan 与浏览器 WebGPU 统一消费公共 Environment Map。
- 桌面主线程在渲染队列满载时继续处理事件和控制状态，但跳过 Scene Snapshot、Draw Binding、
  ImGui Capture 与 Frame Packet 的昂贵构造。

### 兼容性与迁移

- RenderPipeline C ABI 兼容新增 Environment Map 导出；GRENV v2 不再受支持，环境资产必须重新生成
  为带 SHA-256 的 v3。完整步骤见[从 0.9 迁移到 0.10](docs/guides/migrate-0.9-to-0.10.md)。

## 0.9.0 - 2026-09-05

### 新增

- RenderPipeline component 随安装包提供标准 PBR 顶点、片元 `.grshader` 清单及 Vulkan SPIR-V、
  WebGPU WGSL sidecar；CMake package 通过 `granit_RENDER_PIPELINE_ASSET_DIR` 暴露稳定资产目录。
- 增加标准 PBR 资产反射契约测试，固定逐帧、材质、对象和光照四组 Binding 以及 Buffer 最小尺寸。

### 变更

- Model Viewer 删除示例私有 PBR Shader，改为直接消费公共标准 PBR 资产；材质包继续通过稳定内容
  ID 引用同一 Shader Asset。
- 标准 PBR 顶点资产补齐逐帧渲染选项字段，使 CPU 常量布局、WGSL 和 SPIR-V 反射保持一致。

### 兼容性与迁移

- 公共 C/C++ API 和 Shader/材质持久化格式相对 0.8.0 没有变化；0.x Consumer 仍应重新编译并把
  CMake 请求版本更新为 0.9。完整步骤见[从 0.8 迁移到 0.9](docs/guides/migrate-0.8-to-0.9.md)。

## 0.8.0 - 2026-09-05

### 新增

- Core 新增后端无关的运行时 Shader Asset 创建入口；调用方提供 `.grshader` 清单和当前后端
  sidecar 的内存字节，Renderer 负责能力匹配、摘要验证与 Shader 创建，不接管文件 I/O。
- `.grshader` schema 4 自描述 Shader 阶段、入口点和稳定内容 ID；ShaderTools 新增 `pack`，可将
  已验证的 SPIR-V/WGSL 载荷打包为统一资产。

### 变更

- `.grmat` 升级至 v4，改为保存 Shader Asset ID，并通过同步 resolver 获取资产；材质包不再内嵌
  一套独立的 SPIR-V/WGSL 双载荷。
- 内置 Canvas、Model Viewer、Smoke Fixture 与相关测试统一消费 `.grshader` 资产，桌面 Vulkan
  与浏览器 WebGPU 共用相同清单并只部署各自需要的 sidecar。

### 兼容性与迁移

- 本版本修改公共 C ABI 和持久化 Shader/材质格式；0.7 Consumer 必须重新编译，旧资产必须重新
  打包。完整步骤见[从 0.7 迁移到 0.8](docs/guides/migrate-0.7-to-0.8.md)。

## 0.7.0 - 2026-09-05

### 变更

- CMake 包的 0.x 兼容选择由“相同主版本”收紧为“相同次版本”；请求 0.6 的 Consumer 不再静默
  接受 0.7 SDK，调用方必须完成迁移并更新 `find_package` 请求版本。
- 安装包门禁新增 Core-only 隔离以及 RenderPipeline、Window、Input、ShaderTools 的独立请求、
  依赖闭包和缺失 component 检查。
- 明确各可安装 component 的职责、直接依赖和 0.x 成熟度；Core 与高层 RenderPipeline 继续保持
  可组合边界，不把上游场景、任务系统或示例 glTF 加载器纳入公共 SDK。

### 兼容性与迁移

- 公共 C ABI、C++ API 和持久化 Shader/材质格式相对 0.6.0 没有变化，仍要求重新编译 0.x
  Consumer。
- 0.6 Consumer 必须将 CMake 请求版本更新为 0.7；完整步骤见
  [从 0.6 迁移到 0.7](docs/guides/migrate-0.6-to-0.7.md)。

## 0.6.0 - 2026-09-05

### 变更

- ShaderTools 的确定性资产改为 `.granit-shader` 反射清单及同名 `.wgsl`、`.spv` 后端载荷；
  缓存恢复会校验三个文件的长度和 SHA-256，旧的单文件内嵌格式不再读取。
- Shader 资产清单新增后端、代码格式、能力档位和特性位变体表；ShaderTools 和命令行可导出
  全量、仅 Vulkan 或仅 WebGPU 载荷，清单只声明实际发布的变体。
- Renderer 新增后端无关的 Shader 能力快照，公开实际后端、portable 档位和已验证的可选特性位，
  并可按后端、档位、必需特性和确定性优先级为调用方选择兼容 Shader 变体。
- ShaderTools 新增内置目标档位能力查询；CLI 可列出 `vulkan-portable`、`webgpu-portable` 并查询
  其静态特性契约，查询结果不依赖构建机 GPU。
- Shader 资产生成描述可声明必需特性；特性进入缓存键和变体记录，所选目标档位不支持时会在
  写入前返回 `unsupported`。CLI 通过 `--features` 提供相同门禁。
- ShaderTools 新增 HLSL portable 双产物编译入口，通过调用方指定的 DXC 生成 Vulkan 1.3 SPIR-V，
  并以独立的 portable 中间 SPIR-V 经锁定 Tint 生成 WGSL；转换失败时返回完整工具诊断并清理
  不完整产物。命令行 `compile-hlsl` 可生成并按发布后端裁剪对应资产。
- Shader 资产缓存身份新增原始源码语言与内容；HLSL 全后端资产命中时可在启动 DXC/Tint 前恢复
  Vulkan SPIR-V 和 WebGPU WGSL。
- ShaderTools 新增 GLSL portable 编译入口，并接受满足兼容策略的编译器版本；工具版本、输入、
  目标档位和特性共同参与确定性缓存身份。
- 提供带完整许可证与 SHA-256 清单的 Windows/Linux 离线 Shader Toolchain 包，以及下载、校验、
  原子解压和缓存复用脚本；官方 CI 使用严格锁定的工具链包。

### 兼容性与迁移

- Shader 资产格式不再兼容 0.5.0 的单文件内嵌载荷，必须使用 0.6.0 ShaderTools 重新生成。
- Core C ABI 仅兼容新增 Shader 能力查询和变体选择接口，既有导出未删除或改名。
- 完整迁移步骤见[从 0.5 迁移到 0.6](docs/guides/migrate-0.5-to-0.6.md)。

## 0.5.0 - 2026-09-05

### 新增

- Window 新增后端无关的当前状态查询，返回最近已知的内容尺寸、Framebuffer 尺寸与内容缩放，
  便于宿主在首个缩放事件前初始化 UI 和呈现状态。

### 变更

- C++ `granit::result` 改为轻量值结构，新增 `ok()`、`failed()`、`native()`、`message()` 和显式
  `operator bool()`，并移除 `succeeded()`、`failed()` 自由函数；布尔上下文中的 `true` 表示成功，
  C ABI `granit_result` 保持不变。
- 桌面端移除实验性的 Dawn WebGPU Provider、插件 ABI 与依赖构建流程；桌面继续使用 Vulkan，
  WebGPU 支持收敛为 Emscripten 浏览器路径。
- 私有 HAL 集中后端能力发现与 Registry 依赖，并拆分 Render Pipeline 的视图、光照、阴影、
  Tone Mapping 和 Draw 录制职责；公共 C API 不暴露内部后端类型。
- 项目开发版本提升到 0.5.0；0.4.0 SDK 使用者升级当前开发版本时必须重新编译。

### 兼容性与迁移

- 0.5.0 功能范围、完整平台发布矩阵和版本发布均已完成。
- C++ Result、描述结构大小宏和桌面 WebGPU 配置存在源码迁移要求，详见
  [从 0.4 迁移到 0.5](docs/guides/migrate-0.4-to-0.5.md)。

## 0.4.0 - 2026-09-03

### 新增

- Core Renderer 新增 Vulkan、桌面 Dawn WebGPU 与 Emscripten WebGPU 的统一后端选择、能力查询和
  运行状态接口；内部使用私有 HAL 隔离资源、命令、Queue、呈现与 Provider 生命周期。
- Shader 工具链以 WGSL 为权威输入，可生成 Vulkan SPIR-V、反射清单和确定性资产，并提供编译、
  校验与诊断接口。
- WebGPU 后端补齐 Buffer、Texture、Sampler、Bind Group、Graphics Pipeline、Indexed Draw、
  上传、回读、Canvas Swapchain 与浏览器帧循环所需能力。
- Renderer 支持 Dynamic Uniform Buffer Binding，并为 Graphics/Compute Bind Group 绑定增加统一
  的版本化描述、动态 Offset 数组及数量、对齐和范围校验。
- Render Pipeline 新增设备限制查询、MSAA Render/Resolve、FXAA、Specular AA、Mipmap 与各向异性
  过滤质量配置。
- 增加跨后端 Model Viewer，使用示例私有 glTF 加载器、PBR 材质、动态 Uniform Arena、轨道相机、
  ImGui 面板与离线预处理环境光，覆盖 Vulkan、桌面 Dawn 和浏览器 WebGPU。

### 改进

- Renderer Registry 与资源记录改为后端无关实现，Vulkan/WebGPU 平台差异下沉到私有后端目录和
  Provider 工厂；公共源文件不再包含原生图形 API 类型。
- Model Viewer 增加环境光强度与旋转、曝光、质量选项、固定布局、帧时间统计和确定性截图比较。
- Windows、Linux、Dawn 与 Emscripten 工作流覆盖共享/静态安装 Consumer、真实后端 Smoke Test、
  浏览器输入与资源归零检查。

### 兼容性与迁移

- `granit_command_recorder_bind_graphics_groups` 和
  `granit_command_recorder_bind_compute_groups` 改为接收 `granit_bind_groups_desc`；旧参数需要迁移
  到初始化后的描述结构。详见[从 0.3 迁移到 0.4](docs/guides/migrate-0.3-to-0.4.md)。
- 项目开发版本已提升到 0.4.0；0.3.0 使用者必须重新编译，不承诺二进制兼容。
- 新增的 WebGPU、Model Viewer、材质资产与示例环境包仍属于 0.x 实验性范围；不承诺原生 WebGPU
  互操作、glTF 公共 SDK、浏览器多线程渲染或 Android 平台支持。

## 0.3.0 - 2026-08-26

### 新增

- Core 新增 Frame Context、帧槽查询和显式 Buffer flush；RenderPipeline 新增 Canvas 批量追加，
  用于复用真实在途帧槽并减少逐项跨 ABI 调用。
- Renderer validation 诊断可定位代表性的 Buffer 描述错误、失效句柄和跨 Renderer 句柄；结果码
  仍是程序逻辑的稳定依据。

### 修复

- RenderPipeline component 的创建接口把空 Renderer 统一归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并在失败时保持输出句柄为零。
- Buffer、Command Recorder、Frame Context、Sampler、Texture 和 Timestamp Query Pool 的创建接口
  同步采用相同的空 Renderer 语义，C++ 包装与 C 接口保持一致。
- Surface、Swapchain、底层 Pipeline、Window 和 Input 创建接口统一把空父资源及资源字段归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并保持失败输出为零。
- Texture View、Shader、Upload Batch、Recorder 批量提交和 Pipeline Cache 操作补齐相同的
  无效句柄语义，保留空批次等参数形状错误为 `GRANIT_ERROR_INVALID_ARGUMENT`。
- C++ RAII 包装在底层句柄或父资源已失效时，`reset()` 返回 `INVALID_HANDLE` 的同时清空本地
  状态，避免对象继续表现为有效或在析构时重复销毁。

### 工程化

- 独立安装 Consumer 注册为 CTest，并自动补充安装共享库的运行时搜索路径；构建指南可用一条
  `ctest` 命令验证 Core、RenderPipeline、Window 和 Input 的七条 C/C++ 路径。
- RenderPipeline C++ 安装 Consumer 通过公开阶段回调执行真实离屏渲染图，覆盖 Scene Snapshot、
  输出纹理、阶段录制、提交和清理，不依赖源码树资源。
- 文档检查锁定 README、安装 Consumer 和 RenderPipeline 教程的关键 CMake/CTest 命令，避免
  文档入口随构建配置漂移。
- Windows/Linux Actions 的安装 Consumer 统一使用与构建指南相同的 CTest 入口，覆盖共享与静态
  安装矩阵并由测试自身设置运行库路径。

### 兼容性与迁移

- 未删除或改名 0.2.0 的公共 C 导出；新增导出属于兼容扩展。
- `granit_canvas_draw_list_desc` 将一个原保留字段定义为 `frame_slot_count`，
  `granit_canvas_record_desc` 新增 `frame_slot` 并扩大 V1 尺寸。旧代码必须使用当前初始化宏重新编译，
  不应手写结构大小或复用 0.2.0 二进制描述布局。
- 空父资源、失效句柄和跨对象归属错误现在统一返回 `GRANIT_ERROR_INVALID_HANDLE`；只比较
  `GRANIT_ERROR_INVALID_ARGUMENT` 的旧错误分支需要同步接受新分类。
- 完整迁移步骤见[从 0.2 迁移到 0.3](docs/guides/migrate-0.2-to-0.3.md)。0.3.0 仍不承诺稳定
  C ABI 或 C++ 二进制 ABI。

## 0.2.0 - 2026-08-24

### 新增

- 核心 Renderer C ABI、C++20 RAII 包装及 Windows/Linux 共享、静态安装 Consumer 验证。
- RenderPipeline、Window、Input、SDL3 Integration 和 ImGui Integration component。
- Granit 0.1.0 基线上的 Core、RenderPipeline、Window 和 Input C ABI 回归快照，以及
  component 级所有权、错误、线程和扩展契约。

### 修复

- RenderPipeline 的公开描述结构提供固定 V1 尺寸，未知尾部可以按统一规则忽略。
- Input 事件与状态输出按调用方 `struct_size` 容量写入，避免旧结构缓冲区越界。
- 核心 Pipeline 销毁接口统一为空句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。

### 兼容性

- 0.2.0 仍是非稳定版本；0.x 次版本可包含有迁移说明的破坏性变更。
