<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。项目当前仍处于 0.x；`Unreleased`
内容不代表已经发布。版本兼容规则见[版本与兼容策略](docs/reference/compatibility.md)。

## Unreleased

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
