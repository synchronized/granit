<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-13：跨后端模型查看器

## 状态

- 实现状态：已确认，待开始
- 前置依赖：S-12
- 可并行准备：示例级 glTF 解析与测试 Fixture
- 优先级：P1

## 背景与目标

简单三角形无法持续验证资产、材质、相机、输入、UI 和多后端组合。S-13 增加一个编辑器式模型
查看器，使用同一份场景数据和 Granit 公共 API 分别运行在 Vulkan、桌面 WebGPU 与浏览器 WebGPU。
glTF 首阶段只是示例输入格式，加载代码位于 `examples/common/gltf`，不构成已承诺的公共 SDK。

目标包括：

- 加载 Khronos glTF Sample Assets 中许可适合再分发的头盔模型。
- 支持索引 Mesh、法线、切线、UV、基础金属度/粗糙度 PBR 材质和纹理。
- 提供轨道相机、模型自动聚焦、基础灯光、曝光和调试显示。
- 叠加 ImGui 编辑器面板，并在三个目标上复用同一查看器状态与渲染逻辑。
- 建立固定相机截图、功能 Smoke Test 和基础性能指标。

## 非目标

- 不在本任务发布通用 glTF Integration SDK。
- 不支持 glTF 的动画、蒙皮、Morph、压缩扩展或全部材质扩展。
- 不建立 Gneiss 的资产格式、RID、缓存或编辑器工程模型。
- 不把示例加载器放进 `granit` 核心目标或安装导出。
- 不使用许可证限制不明确或不适合再分发的模型作为仓库默认资产。

## 已确认决策

- 示例加载代码放在 `examples/common/gltf`，构建为不安装、不导出的内部目标
  `granit_example_gltf_support`。
- `cgltf` 和图片解码器只作为该目标的私有实现依赖，类型不得出现在 Granit 公共头文件中。
- 加载器输出后端无关的 CPU Scene、Mesh、Primitive、Material 和 Image 数据；GPU 上传由查看器完成。
- 默认评估 `FlightHelmet` 等 CC0 资产；若采用其他模型，必须保存来源、版本、许可证和署名。
- 桌面窗口与输入优先复用 SDL3 Integration；ImGui 继续通过现有 Draw Data 转换入口渲染。
- 浏览器目标复用相同查看器核心，平台层只负责主循环、Canvas、输入与资源获取。

### S-13A 资产与依赖契约

首轮固定使用 `cgltf` 1.15 解析 glTF/GLB，使用 `stb_image` 2.30 解码 PNG/JPEG。
两者均以确切 Git commit 和 SHA-256 锁定，由 `granit_example_gltf_support` 私有编译；
不安装头文件、不导出 CMake Target，不向 `granit` 传递 Include 目录或编译定义。

- `cgltf` 采用 MIT 许可；`stb_image` 按其 MIT 选项纳入第三方通知，并仅开启
  PNG/JPEG 解码以收窄攻击面与产物大小。
- 完整示例资产采用 Khronos glTF Sample Assets 的 `FlightHelmet` GLB 变体，许可为
  CC0-1.0。仓库记录上游提交、原始 URL、SHA-256 和许可文本。
- 完整头盔不进入默认 Git 工作树；通过显式 CMake 选项或辅助脚本下载到构建
  缓存，校验 SHA-256 后使用。离线构建可指向已验证的本地资产路径。
- 一个仓库内 CC0 小型 GLB Fixture 用于解析、错误语义和 Smoke Test；手动下载失败
  不得导致默认 Granit 库构建失败。
- 依赖获取顺序为父项目 Target、`find_package`、锁定的内置回退；不使用浮动
  `main`/`master` 分支。更新时单独审核许可、安全修复、产物大小和三平台构建。

S-13A 的交付物包含依赖锁定记录、第三方通知、资产 manifest、可重入的获取
脚本和离线路径验证。没有通过哈希与许可校验时，不得开始将完整资产接入查看器。

### S-13B CPU Scene 与加载契约

实现位于 `examples/common/gltf`，建议拆分为 `scene.h`、`loader.h/.cpp`、
`image_decoder.h/.cpp` 和目录内 `CMakeLists.txt`。`scene.h` 是示例私有的 CPU 数据契约，
不安装且不包含 `cgltf_*`、GPU 句柄或后端类型。

CPU Scene 包含以下所有权对象：

- `scene`：拥有 Node、Mesh、Material、Image 和 Sampler 数组及根 Node 索引。
- `node`：保存名称、父子索引、可选 Mesh 索引、本地矩阵和已求值的世界矩阵。
- `mesh` 与 `primitive`：保存 Primitive 范围、统一为 `uint32` 的索引、材质索引、
  AABB，以及 Position/Normal/Tangent/UV0 语义数组；不保留 glTF Accessor 指针。
- `material`：保存 Base Color、Metallic、Roughness、Normal Scale、Occlusion Strength、
  Emissive Factor 以及五类 PBR Texture 引用。
- `image`：拥有解码后的 RGBA8 像素、尺寸与 mip 链；颜色空间由 Material 使用
  语义决定，不固化在可能被多处引用的 Image 上。
- `sampler`：保存 glTF Filter 与 U/V Wrap；转换为 Granit Sampler 由 S-13C 负责。

加载入口接受只读字节 Span 并填充输出 Scene，不直接访问文件系统；因此桌面文件、
浏览器 Fetch 和内嵌 Fixture 可复用同一解析路径。首轮只接受 GLB 与其内嵌 Buffer/Image，
外部 URI、Data URI 和网络获取不进入解析器。

- 坐标保持 glTF 右手、Y 向上语义；矩阵转为 Granit 列主序数值，不翻转顶点、
  索引绕序或纹理 V 坐标。剪裁空间差异由 Renderer/Shader 契约处理。
- 只支持 Triangle Primitive、UV0、`uint8/uint16/uint32` 索引与 glTF 允许的对应
  顶点分量类型；Sparse Accessor、Draco/Meshopt、UV1、Skin、Animation 与 Morph 明确拒绝。
- Position 与 Normal 必须存在；使用任意 Texture 时必须存在 UV0，使用 Normal Texture
  时必须存在 Tangent。首轮不在加载期自动生成法线或切线。
- 解析成功后 Scene 不借用输入 Span 或第三方内存；失败保持输出不变。
- 错误由示例私有 `load_error` 与可选诊断文本返回，区分非法 GLB、截断数据、
  越界 Accessor、不支持 Feature、图片解码失败、数值溢出与内存不足。

单元测试覆盖 Node 层级、TRS/矩阵、交错 Accessor、索引归一化、材质纹理语义、
图片解码、AABB 与上述所有错误。测试只使用仓库内小型 Fixture，不下载头盔。

## 实施顺序

1. **S-13A 资产与依赖评估**：锁定模型、`cgltf`、图片解码器版本、许可证及获取/打包方式。
2. **S-13B 示例加载器**：解析 GLB、节点变换、Primitive、索引、顶点属性、PBR 材质和图片。
3. **S-13C GPU 场景**：创建 Mesh、纹理、Sampler、材质、动态 Uniform Arena 和逐帧绘制数据。
4. **S-13D 查看器交互**：实现轨道相机、自动聚焦、调整窗口和鼠标/键盘操作。
5. **S-13E ImGui 面板**：显示层级、材质、灯光、曝光、后端、FPS 与 CPU/GPU 帧时间。
6. **S-13F 三端接入**：运行 Vulkan、桌面 Dawn WebGPU 和 Emscripten WebGPU 查看器。
7. **S-13G 验收**：固定相机截图、错误资产测试、Smoke Test、性能记录和使用指南。

## 测试与验收

- 加载器覆盖有效 GLB、截断数据、非法索引、缺失属性、不支持扩展和图片解码失败。
- CPU Scene 不包含后端类型，同一加载结果可分别上传到 Vulkan 与 WebGPU Renderer。
- 三个目标均可显示模型、操作相机并渲染 ImGui；后端差异只通过能力查询处理。
- 固定相机下的轮廓、深度和关键材质区域满足量化容差；截图差异不得通过扩大阈值掩盖。
- Smoke Test 使用小型仓库 Fixture；完整头盔资产不应让普通构建和 CI 重复下载大文件。
- 文档记录模型来源、许可证、依赖版本、启动命令和已知平台限制。

## 风险与未决问题

- 浏览器 ImGui Platform Backend、字体和模型资源加载需要与 Emscripten 主循环共同验证。
- PBR 最终画面还受色彩空间、切线、Mip、环境光和 Tone Mapping 差异影响，验收应分层比较。
- 完整模型适合按锁定版本下载或使用可选资产包，具体缓存策略在 S-13A 决定。
- 当至少两个非示例 Consumer 需要复用加载器，且 CPU 数据模型经过查看器验证后，再启动 S-14，
  将其提升为正式 `granit::integration_gltf`；否则继续保持示例私有实现。
