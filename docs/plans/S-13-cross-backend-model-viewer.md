<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-13：跨后端模型查看器

## 状态

- 实现状态：进行中；S-13A、S-13B 已完成，当前实施 S-13C
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

- `cgltf` 锁定提交 `360db1a95480fe102ae9c69b27c5d101167ff5ba`，源码归档 SHA-256 为
  `445d135cf793232ae6a585ca1404e4ff28d4f4dbca070689034fe780370ac84a`。
- `stb_image` 锁定首次标记 2.30 的提交 `013ac3beddff3dbffafd5177e7972067cd2b5083`，源码归档
  SHA-256 为 `b01aa93e1a968aed55f43e072c98ee401d2f20e897aabdb1a166c7166886ed11`。

- `cgltf` 采用 MIT 许可；`stb_image` 按其 MIT 选项纳入第三方通知，并仅开启
  PNG/JPEG 解码以收窄攻击面与产物大小。
- 完整示例资产采用 Khronos glTF Sample Assets 的 `FlightHelmet` glTF 变体，许可为
  CC0-1.0。上游没有提供该模型的 GLB 变体；仓库记录上游提交、原始 URL、逐文件
  SHA-256 和许可文本。
- 完整头盔不进入默认 Git 工作树；通过显式 CMake 选项或辅助脚本下载到构建
  缓存，校验 SHA-256 后使用。离线构建可指向已验证的本地资产路径。
- 一个仓库内 CC0 小型 GLB Fixture 用于解析、错误语义和 Smoke Test；手动下载失败
  不得导致默认 Granit 库构建失败。
- 依赖获取顺序为父项目 Target、`find_package`、锁定的内置回退；不使用浮动
  `main`/`master` 分支。更新时单独审核许可、安全修复、产物大小和三平台构建。

S-13A 的交付物包含依赖锁定记录、第三方通知、资产 manifest、可重入的获取
脚本和离线路径验证。没有通过哈希与许可校验时，不得开始将完整资产接入查看器。
资产清单位于 `examples/assets/FlightHelmet.manifest.json`，获取入口为
`cmake/fetch_flight_helmet.cmake`；默认构建不会调用该脚本或访问网络。

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

加载入口接受主文档字节 Span 和只读资源解析回调并填充输出 Scene，不直接访问文件系统；因此桌面
文件、浏览器 Fetch 和内嵌 Fixture 可复用同一解析路径。首轮接受 GLB 内嵌 Buffer/Image，以及
`.gltf` 中不含 Scheme、查询、Fragment、绝对路径或父目录跳转的相对 URI。Data URI 和网络获取
不进入解析器；资源层必须在调用 Loader 前完成获取，并由回调按规范化 URI 返回只读字节。

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

S-13B 已完成：`granit_example_gltf_support` 保持示例私有，固定 GLB Fixture、内存 GLB、
外部资源与错误矩阵均由 `granit.example.gltf_support` 覆盖，不需要网络或文件系统。

### S-13C GPU Scene 与逐帧数据契约

GPU 侧适配位于 `examples/common/model_viewer`，建议拆分为 `gpu_scene.h/.cpp` 与
`frame_data.h/.cpp`。它只组合已安装的 Granit C++ 公共 API，不包含 `src/` 私有头、
Vulkan、Dawn 或 Emscripten 类型。

`gpu_scene` 与一个 Renderer 绑定，并拥有以下 move-only 资源：

- 将全部 Primitive 打包进少量不可变 Vertex/Index Buffer；每个 `granit::mesh` 保存
  对应 Offset 和 Draw 范围，不为每个 Primitive 执行独立 GPU 分配。
- 每个唯一 Image/颜色空间组合对应 Texture 与默认 View；同一 Image 同时作为
  sRGB 和线性资源时创建两个显式格式资源，不在 Shader 中补偿。
- Sampler 按规范化后的 Filter/Wrap 去重；材质使用 `granit::material_instance`、构建期生成的
  双后端 PBR 材质归档及批量初始参数，缺失纹理使用 Granit 的 PBR 默认资源。
- 每个 Node/Primitive 实例生成稳定 payload，映射到 `granit_mesh` 与 `granit_material`；
  世界矩阵、法线矩阵与世界空间 Bounds 通过 `granit::scene_snapshot` 提交。

上传是事务式的：先在候选 `gpu_scene` 中创建所有句柄，再使用一个 Upload Batch 写入
Buffer 与全部 Texture mip，只在提交成功后替换旧 Scene。失败时按 Mesh、Material、
Sampler、View、Texture、Buffer 的依赖逆序销毁候选资源，原 Scene 保持可用。

逐帧常量不在示例中创建另一套渲染入口。`granit_render_pipeline` 内部的现有
逐 Draw Uniform 资源升级为通用动态 Uniform Arena：

- 根据 `frames_in_flight` 创建帧槽；每个帧槽独立保留 Buffer 区域、写入游标和
  Bind Group，只在 Frame Context 返回该槽后重用。
- 用 `granit_renderer_get_limits` 的 Uniform Offset 对齐对 Frame/Object 常量块取整；
  所有尺寸计算做溢出检查，单块不得超过最大 Uniform Binding 大小。
- 每帧使用一个 Upload Batch 写入常量区域，录制 Draw 时只传递动态 Offset；空间
  不足时按受控倍率增长，不在每个 Draw 创建 Buffer 或 Bind Group。
- Arena 是 Render Pipeline 的私有实现，不形成新公共句柄；后续有第二个非内部
  Consumer 需求时再单独评估提升。

测试覆盖打包 Offset、去重、线性/sRGB 分裂、完整 mip 上传、事务回滚、销毁顺序、
跨 Renderer 拒绝、帧槽重用、对齐/溢出和 Arena 增长。同一 CPU Scene 必须可在两个
Renderer 上分别创建独立 GPU Scene，不共享任何 GPU 句柄。

当前进度：GPU Scene 已完成合并 Vertex/Index Buffer、纹理颜色空间拆分、Sampler 去重、
单变体 PBR 材质实例、默认纹理、稳定 Draw Binding 与 Renderable 打包；调用方可使用逐帧
View/Light 创建不可变 `scene_snapshot`。Render Pipeline 的 Frame/Object 布局已切换为动态
Uniform Binding，带 Swapchain Frame 的路径按真实帧槽复用 Arena Buffer 与 Bind Group；普通离屏
提交暂时保留独立 Buffer 兼容路径。批量上传、增长/延迟回收专项测试与跨 Renderer 图形 Smoke
Test 仍属于本阶段后续工作。

#### S-13C1 双后端材质归档前置

状态：已完成。

现有材质归档 v2 只保存 SPIR-V。虽然公共 Shader 描述已经能同时携带 SPIR-V 与 WGSL，
`material_template_gpu` 仍只能从归档创建 Vulkan Shader。模型查看器不能复用该归档在 WebGPU
运行，也不能包含 `src/pipeline` 的嵌入材质或为不同后端绕过 Material/Render Pipeline。

S-13C 先完成一次聚焦的材质资产升级：

- 材质 Shader 记录同时保存 SPIR-V 与 WGSL，运行时继续通过同一公共 Shader 描述交给所选后端；
  Material API、句柄和调用方式不变，不增加后端条件分支。
- 材质源 JSON 的每个 Shader 显式引用锁定的 `.spv` 与 `.wgsl` 文件；构建工具校验阶段、入口点、
  空数据、大小和重复记录，归档不在运行时调用 Tint 或读取源文件。
- 归档格式只提升一次并同步工具、调试 JSON、Fixture、示例与格式测试；项目仍处于早期设计阶段，
  不在运行时保留只含 SPIR-V 的旧格式分支。
- 模型查看器 PBR Shader 以 WGSL 为权威输入，由锁定 Tint 在构建期生成匹配 SPIR-V；生成物和
  材质归档只属于示例构建，不安装为 Granit 公共资源。
- Vulkan、WebGPU Mock 与真实 Dawn 至少各验证一次同一归档；普通 Granit 构建不启用模型查看器时
  不下载 Tint、不生成示例归档。

该前置只扩展内部材质资产，不修改 C ABI 版本。完成后 `gpu_scene` 才创建
`material_instance`、稳定 Payload 和 `scene_snapshot`。

### S-13D 查看器交互契约

交互核心位于 `examples/common/model_viewer/orbit_camera.h/.cpp`，只接收规范化的
`viewer_input` 和帧缓冲像素尺寸。SDL3 事件、浏览器事件与 ImGui Capture 决策由
平台壳转换，相机类不包含 SDL、Emscripten 或 ImGui 头文件。

`orbit_camera` 保存 Target、Distance、Yaw、Pitch、垂直 FOV 和 Near/Far，并输出 Granit
右手 View 矩阵与 `[0,1]` 深度投影矩阵。所有状态必须有限，Pitch 限制在两极之内，
Distance、Near/Far 由模型尺度推导并保持严格有效。

- 初始与 `F` 键聚焦合并后的世界空间 Bounds：Target 取中心，Distance 根据球半径、
  垂直/水平 FOV 中较紧的一边和 10% 边距计算。空 Scene 使用有限的默认相机。
- 鼠标右键拖动环绕，中键拖动在相机 Right/Up 平面平移，滚轮按指数比例缩放，
  `F` 聚焦当前选择或整个 Scene，`Home` 恢复初始视图。首轮不承诺触摸手势。
- 拖动量除以帧缓冲高度，平移再按 Distance 与 FOV 缩放；因此高 DPI、
  CSS Canvas 尺寸和桌面窗口尺寸不会改变单位拖动的视觉敏感度。
- ImGui 声明捕获鼠标或键盘时，平台壳不向相机转发对应操作；焦点丢失、
  指针离开或按键释放时清理拖动状态，防止粘滞输入。
- 像素尺寸变化只重算 Aspect 和 Projection；Swapchain 重建由平台壳处理。零尺寸时
  暂停渲染但继续消费退出/恢复事件，恢复后不累积鼠标 Delta。

相机单元测试使用固定输入序列，覆盖 Bounds 聚焦、空 Scene、极端 Aspect、Pitch/Distance
限制、高 DPI 等价、ImGui 捕获、焦点丢失和零尺寸恢复。Smoke Test 需在调整
窗口前后验证模型仍位于视野内，且不依赖绝对屏幕坐标。

### S-13E ImGui 与性能指标契约

`viewer_state` 是示例唯一的可编辑状态，拥有选中 Node/Material 索引、相机、
方向光、曝光、调试显示与面板开关。ImGui Widget 只生成有类型的状态更改；资源更新在
UI 构建结束后事务式应用，不在 Widget 调用中创建或销毁 GPU 资源。

首轮面板结构固定为：

- **Scene**：Node 层级、可见状态与选择；选择失效时回退为未选中。
- **Inspector**：只读 Transform/Mesh 信息，以及 PBR Factor、Texture 缩略图和 Sampler 信息；
  材质修改通过批量 `material_instance::update` 提交，失败保留上一有效值。
- **Lighting**：方向、辐亮度、曝光和调试模式；输入保持有限并限制在文档范围。
- **Renderer**：只读显示实际后端、Adapter、Swapchain 格式/尺寸、Present Mode、帧槽数和
  能力；不允许运行中原地替换 Renderer。
- **Performance**：显示平滑 FPS、CPU 帧时间、帧槽等待、Present 等待与 GPU 阶段时间；
  默认保留 240 个原始样本，显示 p50/p95 和最大值，不用单次峰值宣称性能。

ImGui 渲染复用 `granit::integration::imgui::append_draw_data` 和与 Renderer 帧槽数一致的
Canvas Draw List。Texture Resolver 只映射字体 Atlas 与已存活的查看器 Texture View；
未知或已销毁 ID 返回错误，不解引用指针型 `ImTextureID`。

为避免示例包含现有私有 `render_pipeline_metrics.h`，S-13E 将通用指标查询提升到
`include/granit/pipeline/render_pipeline.h/.hpp`：

- 可扩展结构返回样本序号、Shadow、Opaque、Tone Mapping 和 Total GPU 纳秒，
  C API 与 C++ 包装同步更新，不公开 Timestamp Query 的后端实现。
- Query Pool 按帧槽隔离，只读取已完成槽的结果；未完成返回 `NOT_READY`，后端
  不支持返回 `UNSUPPORTED`。UI 显示“不可用”，不伪造 0 ms 且不中止查看器。
- CPU 墙钟、GPU Timestamp 与等待时间分开报告，不相加为“总帧时间”。性能导出
  记录构建类型、Validation、后端、Adapter、驱动、尺寸、Present Mode 和帧槽数。

测试覆盖面板状态转换、材质回滚、Texture ID 失效、ImGui 捕获、指标帧槽延迟、
`NOT_READY/UNSUPPORTED` 和样本统计。图形 Smoke Test 只检查面板可见与 Draw Data 非空，
不依赖字体光栅的逐像素结果。

### S-13F 三端启动与资源流程

共享应用核心构建为不安装的 `granit_model_viewer_core` 静态目标，拥有 Loader、
CPU/GPU Scene、Orbit Camera、Viewer State、ImGui 面板与单帧 `tick`。它不提供 `main`、
不创建原生窗口，也不包含桌面或浏览器条件编译。

三个运行配置如下：

- **桌面 Vulkan** 与 **桌面 Dawn WebGPU** 共用一个 `granit_model_viewer_example`；
  SDL3 拥有 Window/事件，`integration_sdl3` 创建 Surface，`--backend=vulkan|webgpu|auto`
  只修改 Renderer 选择。显式选择 WebGPU 时不回退 Vulkan。
- **浏览器 WebGPU** 构建 `granit_webgpu_model_viewer_example.html`；Renderer 与 Provider 静态链接，
  Canvas Surface、`requestAnimationFrame` 主循环及 Keyboard/Pointer/Wheel 事件留在 `web/`
  薄壳中。壳层将事件同时送入 ImGui IO 和规范化 `viewer_input`。

资源来源抽象为只读字节请求，完成后才调用 S-13B Loader：桌面壳异步读取
`--asset=<path>`；浏览器壳通过 Fetch 读取 URL。小型 Smoke Fixture 可预加载，
完整 FlightHelmet 作为独立缓存资源提供，不嵌入 Wasm/JS 或桌面可执行文件。

启动是显式状态机：`platform_ready`、`renderer_pending`、`asset_loading`、
`gpu_upload`、`ready`、`failed`。WebGPU 设备异步创建期间不进入 Scene 上传；失败页面/窗口
保留诊断文本、Granit 结果码和实际后端，不仅输出黑屏。

- 每帧顺序固定为：事件转换、ImGui New Frame、状态更新、Scene Snapshot、ImGui
  Draw Data 转换、Acquire/Frame Context、Render Pipeline、Submit/Present 与延迟指标读取。
- 桌面以 `SDL_GetWindowSizeInPixels` 为 Swapchain 尺寸；浏览器根据 CSS 尺寸与
  `devicePixelRatio` 设置 Canvas 像素尺寸。尺寸变化只在帧边界重建 Swapchain。
- Device Lost、Surface Lost 与 Out Of Date 分别进入 Renderer 失败、Surface 重建和
  Swapchain 重建路径；不将三者泛化为一次无限重试。
- 退出时先停止新帧/Fetch 回调，再等待 Renderer 提交完成，按 UI、GPU Scene、
  Pipeline、Swapchain、Surface、Renderer、Window 顺序释放。异步回调不得捕获已销毁状态。

桌面目标只在 SDL3、ImGui 与对应 Integration 可用时构建；浏览器目标只在
Emscripten 预设构建。两者都不进入 Granit 安装导出，且不复制一份 Renderer Registry
或 WebGPU Provider 实现到示例目录。

### S-13G 验收与回归契约

验收分为四层，不用一个可视化窗口 Smoke 代替可定位的单元和像素回归：

1. **CPU 契约**：Loader、Scene、Camera、UI State、资源打包与统计在无 GPU 环境运行。
2. **离屏 GPU Fixture**：仓库内小型 GLB 在 Vulkan 与桌面 Dawn 上通过同一公共路径
   上传和绘制，不创建窗口；浏览器在 Chromium WebGPU 中运行同一 Fixture。
3. **交互 Smoke**：运行 60 帧，注入环绕、缩放、选择、ImGui 修改和一次 Resize，
   验证实际后端、非空 Draw Data、有限相机与无 Validation/浏览器 Console 错误。
4. **FlightHelmet 验收**：手动 Actions 下载并校验锁定资产，使用固定相机、光源、
   曝光、尺寸和时间生成无 UI 截图，另行验证带 UI 的启动截图。

固定截图使用 512×512 RGBA8 输出，在资源上传后预热至少三帧再回读。比较分层进行：
背景/轮廓遮罩允许一像素边缘容差，深度先验证前后关系，再对非边缘颜色统计
平均绝对误差与超阈值像素比例。不使用整图精确哈希，也不通过扩大阈值掩盖
稳定的材质、法线、sRGB 或深度差异。

- Windows 手动 Actions 覆盖 MSVC/Clang、Vulkan 与 Dawn D3D12；Linux 覆盖 GCC/Clang、
  Vulkan 与 Dawn Vulkan，并复用 X11/Wayland Integration Runtime；Emscripten 使用锁定
  Chrome/Emdawnwebgpu 运行浏览器 Fixture 和交互 Smoke。
- 截图失败时上传实际图、期望图、差异图、模型 manifest、Renderer/Adapter
  信息和诊断日志；成功时仅保留简短汇总，不重复上传大资产。
- Release 性能基线固定 1920×1080、相机和 FlightHelmet，分别测量 UI 开/关与
  Immediate/FIFO；预热 300 帧、采样 1000 帧，报告 CPU/GPU/等待的 p50/p95/p99。
  首份数据建立基线而非不经复测的硬性 FPS 门槛。

S-13 只在上述手动 Actions 全部通过，公共头/安装 Consumer 通过，并完成示例指南、
依赖/资产许可参考与带日期的验收 Record 后标记完成。验收结果再决定是否启动
S-14，不在 S-13 实施中预先安装或导出 glTF 加载层。

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
