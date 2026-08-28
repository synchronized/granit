<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 架构与 ABI

## 当前稳定性状态

项目处于早期开发阶段，本文描述的是当前设计方向，而不是已经冻结的兼容承诺。公共 C API、
C++ 包装、结构体布局、符号集合和行为语义均可进行破坏性修改，不要求兼容仓库的旧提交或尚未
发布的构建产物。

C ABI 分层仍作为架构约束，用于隔离动态库和 C++ 实现细节；这不表示当前 ABI 已经稳定。正式
版本发布前将另行确定版本协商、弃用周期、符号兼容和结构体扩展策略。

## 产品定位

Granit 定位为基于 Vulkan 的中层、显式、可嵌入式渲染库，主要服务自研游戏引擎、实时应用和
图形工具。它隐藏 Vulkan 的实例、设备、同步和资源管理细节，但保留现代图形 API 中明确的资源
用途、命令记录、提交和生命周期语义。

核心库采用“Bring Your Own Engine”边界，不直接拥有使用者的 Scene、Entity、Camera、Light、
动画或资产数据库。PBR、场景渲染、后处理套件和 Render Graph 可以在核心能力稳定后作为独立
高层模块提供，不能反向污染稳定 C ABI。

当前 Vulkan 后端覆盖完整生产路径，WebGPU 后端已覆盖 0.4.0 浏览器 MVP 所需的基础资源、Pipeline、
命令与呈现闭环。两者通过私有 HAL 共享 Registry，但不承诺能力完全对称；Direct3D、Metal 和
OpenGL 当前不在支持范围内。

## 渲染目标模型

离屏渲染是核心能力，不是窗口渲染的附加功能。Swapchain 只是外部输出端点，不能成为资源、
命令或帧流程的中心。预期关系为：

```text
Texture
  └─ Texture View
       └─ Render Target Attachment
            ├─ 离屏 Texture
            └─ Swapchain Backbuffer
```

渲染命令面向统一的 Render Target Attachment；调用者无需根据目标来自离屏 Texture 还是
Swapchain 图像选择两套命令。未启用 Surface 的 Renderer 仍应支持资源上传、计算和完整离屏渲染。

该模型必须覆盖阴影贴图、后处理、编辑器 Viewport、反射探针、缩略图、图像回归测试和无窗口
资源烘焙。Swapchain 图像作为内部非拥有 Texture/View 接入，普通资源销毁接口不得释放它们。

## 架构参考

- 主要参考 Diligent 的资源、View、Pipeline、Device Context 和资源状态表达，但不照搬 COM
  接口、引用计数对象和多后端工厂体系。
- 参考 bgfx 的整数句柄、C API、可嵌入边界、批量提交和多线程 Encoder，但不继承为兼容旧图形
  API 形成的限制，也不采用依赖全局状态的使用方式。
- 后期参考 Filament 的 FrameGraph、临时资源生命周期、材质和 PBR 分层；Scene、Camera、Light
  和完整 PBR 工作流不进入 Granit 核心层。

参考项目用于比较职责边界和成熟设计，不要求保持 API 兼容，也不以逐项复刻为目标。

## 渲染路径定位

Granit 核心 `Renderer` 是后端中立的显式 GPU 接口，本身不限定 Forward 或 Deferred。当前 H-07
参考渲染套件采用 **Forward PBR** 路径：先执行 Scene 可见性筛选和方向光 Shadow Pass，再在一个
PBR HDR Pass 中直接完成材质着色与光照，最后执行 Tone Mapping 输出显示空间图像。

```text
Scene 可见性筛选
  -> Directional Shadow Pass
  -> Forward PBR HDR Pass
  -> Tone Mapping
  -> 最终输出
```

当前实现不是 Deferred Renderer：没有 G-Buffer，也没有独立的屏幕空间 Deferred Lighting Pass。
当前实现也不称为 Forward+ 或 Clustered Forward：虽然已有逐 View 光源可见性和有界批量打包，
但尚未实现屏幕 Tile/Cluster 划分、GPU 光源分配或按 Cluster 查询光源列表。

后续优先在现有 Forward PBR 基础上演进为 Clustered Forward，以继续复用材质、透明物体、多 View
和 MSAA 路径。Deferred Rendering 可以作为使用 Render Graph 组合的可选高级管线，但不替代核心
Renderer，也不要求现有参考管线改用 G-Buffer。只有完成 Clustered Light Culling 并通过多光源
性能与像素验证后，文档和产品名称才使用 Forward+ 或 Clustered Forward。

类比主流项目时，核心 Renderer 的职责边界更接近 Diligent、WebGPU 或 bgfx 一类显式资源与命令
封装；H-07 高层参考管线则接近精简的 Filament/DiligentFX 风格画质套件。该类比只描述分层和
渲染路径，不表示 API、Shader、材质或资产格式兼容。

## Renderer 与高级渲染套件

Granit Renderer 的长期职责止于 GPU 资源、Pipeline、Bind Group、命令、同步、提交、Swapchain 和
GPU 生命周期。它不负责选择 PBR 模型、筛选 Scene、生成阴影、管理环境光照或执行 Tone Mapping。

Material、Scene、PBR、Lighting 和 Post Process 由 `granit::render_pipeline` 组合成类似
DiligentFX 的可选高级参考渲染套件。Render Graph 当前是该套件的内部实现模块，没有公共 C ABI
或 C++ API。统一门面提供默认可运行路径，但不是核心 Renderer 的唯一入口。

使用者当前可以选择三种逐级开放的层级：

1. 使用完整参考管线，只提交 Scene、View、目标和质量配置，获得阴影、PBR、IBL 和后处理的
   默认组合。
2. 配置或扩展参考管线，使用自定义 Material，并在稳定扩展点插入后处理、UI 等 Pass。
3. 直接使用核心 Renderer，自行管理 GPU 资源、命令、同步和提交。

自行组合高层模块或构建 Render Graph 属于未来可能公开的扩展层级；在形成真实需求和独立 ABI
设计前，不应把内部头文件或实现目标作为公共接口使用。

各层职责保持正交：PBR 决定材质与光照如何着色；Render Pipeline 决定一帧包含哪些渲染阶段；
Render Graph 根据 Pass 的资源读写声明组织依赖、顺序和资源状态；Renderer 负责执行后端中立的
GPU 命令。Render Graph 不内置 PBR、阴影或 UI 业务语义，Renderer 也不接管整帧策略。

```text
Scene / View / Material
          -> Render Pipeline（整帧策略）
          -> 内部 Render Graph（Pass 与资源依赖）
          -> Renderer / Command Recorder（GPU 执行）
          -> Vulkan / WebGPU（内部后端）
```

依赖始终从高级层指向核心层。核心动态库不能链接高层目标，也不能为 `render(view)` 接管 ECS、
Scene Graph、Camera、Light、Mesh、Material 或资产数据库。高级套件可以拥有自身 GPU 缓存和中间
资源，但只借用调用方快照、payload、外部目标与环境资源。

PBR 质量体系主要参考 Filament；底层资源组织和高层组件分离主要参考 Diligent/DiligentFX。项目
不兼容它们的 API、对象模型、Shader、材质包或资产格式，也不复制 COM 引用计数或整体 Engine
所有权。具体实施条件见 [H-07 计划](../plans/H-07-reference-render-pipeline.md)；无光照、2D 与 UI
路径见 [H-06 计划](../plans/H-06-unlit-2d-ui.md)。

## 分层

Granit 使用三层接口隔离使用者与具体图形后端：

1. `.hpp` C++20 API：面向普通用户，提供强类型、移动语义和 RAII。
2. `.h` C API：动态库的稳定 ABI，也是其他语言绑定的基础。
3. 内部实现：资源表、渲染调度、私有 HAL 与 Vulkan/WebGPU 后端，不进入公共头文件。

C++ 包装层保持轻量，不维护一套独立的渲染状态。它拥有或引用 C 句柄，并将所有实际操作转发给
C API。普通 C++ 用户不需要直接使用 C API。

### Renderer 私有 HAL

Renderer 内部在公共 Registry 与具体图形 API 之间使用 `src/backend` 私有 HAL：

```text
公共 C/C++ API
  -> Renderer Registry（句柄、所有权、生命周期、公共校验）
  -> 私有 backend_* HAL（能力、资源、命令、Queue、呈现契约）
  -> Vulkan / WebGPU 平台实现
```

HAL 是一组按职责拆分的粗粒度内部接口，不是新的公共 API，也不是 Vulkan 接口的逐项翻译。
Registry 不保存 `Vk*` 或 `WGPU*` 类型，不负责后端同步和描述转换；后端实现也不复制公共句柄表、
资源父子关系或 C ABI 校验。高频 Draw、资源访问和提交继续采用命令记录或批量契约，避免为每个
细粒度操作增加动态库或虚函数调用。

Registry 只通过 `backend_shader_renderer`、`backend_pipeline_renderer` 和
`backend_presentation_renderer` 等职责接口访问 WebGPU，不直接取得 WebGPU Adapter 对象。WebGPU
支持的 Pipeline 描述范围和格式转换由 Pipeline Adapter 判断；Dawn 与 Emscripten 的 Provider
差异也不得回流到公共 API 或 Registry。

桌面与 Emscripten 复用同一组 Registry 编译单元，不维护浏览器专用的资源表或命令实现。
Renderer 创建按后端拆成独立工厂编译单元：桌面默认工厂构造 Vulkan 状态，浏览器工厂静态绑定
WebGPU Provider；平台选择不进入通用 Registry 实现。

Vulkan 与 WebGPU 不必提供完全对称的内部能力。共同语义由 Registry 校验，设备差异通过不可变
能力快照和统一结果码表达；无法安全模拟的能力明确返回不支持。该边界的决策依据见
[ADR-003：Renderer 内部多后端边界](../decisions/ADR-003-internal-renderer-backend-boundary.md)。

### 操作系统平台层

`src/platform` 集中保存操作系统和原生窗口系统实现。Window Registry 只管理公共句柄、线程规则、
事件队列和后端分派；Win32、XCB 与 Wayland 的原生窗口生命周期及事件泵分别位于对应平台编译单元。
`src/input` 只保留公共 Input 状态、UTF-8 处理和事件分派，统一平台输入门面再调用对应平台解码器。
动态库外观与所有权位于 `platform/shared_library.*`，`LoadLibrary` 和 `dlopen` 实现分别位于 Win32
与 POSIX 编译单元，避免通用实现文件包含系统 Loader 头文件。

`src/integrations` 不承担操作系统抽象，只保存 SDL3、ImGui 等第三方库与 Granit 公共接口之间的
可选适配。平台层不得依赖这些集成目标；集成层可以调用 Granit 的 Window、Input 或 Renderer
公共 API。

## 数学值类型与内部运算

公共目录 `granit/math` 提供 C ABI 普通值类型 `granit_float2/3/4` 和列主序
`granit_matrix4`；C++20 通过 `granit::math::float2/3/4` 与 `granit::math::matrix4` 使用同一布局。
Scene、Material、PBR 和 Lighting 不应各自维护含义相同但类型不同的数学值。

这些公共类型只用于跨 API/ABI 搬运数据，不建立大型数学库，不要求使用者放弃 GLM 或自有数学
类型。可选高层模块共享的小型内部目标 `granit::math` 在相同值类型上提供基础运算，以及构建阴影
和相机矩阵所需的最小函数；运算函数当前不是公共 ABI。

数学层统一使用右手坐标系，矩阵与 HLSL `column-major float4x4` 一致，投影深度范围为 Vulkan 的
`[0,1]`。`look_at_rh`、`perspective_rh_zo` 和 `orthographic_rh_zo` 会拒绝退化或非有限参数，
失败时不修改输出。

现阶段不加入四元数、Transform 层级、SIMD、双精度或通用几何容器，只有实际模块产生需求后才
扩展。公共数学值类型的当前稳定等级见[兼容策略](../reference/compatibility.md)。

## Vulkan 封装边界

公共头文件必须满足以下约束：

- 不包含 Vulkan SDK 头文件。
- 不声明或返回 `Vk*` 类型。
- 不要求使用者链接 Vulkan loader。
- 不要求使用者了解队列族、命令池、描述符池或 Vulkan 同步细节。

内部使用 Vulkan-Headers 1.4.350 与匹配的 Volk 1.4.350。Volk 以 object library 形式并入
Granit，启用 `VK_NO_PROTOTYPES` 和 C++ namespace，不直接链接 `vulkan-1`。运行时由 Volk
查找系统 Vulkan loader；头文件版本不改变 Granit 以 Vulkan 1.3 为最低运行能力的目标。

Loader 初始化状态在进程内缓存。每个 Vulkan instance 使用独立 `VolkInstanceTable`，后续每个
device 使用独立 `VolkDeviceTable`，不通过 Volk 全局 instance/device 加载函数共享可变分发表。

首期设备要求为 Vulkan 1.3 graphics queue、dynamic rendering、synchronization2 和 maintenance4。
设备类型优先级高于显存大小，枚举顺序作为最终稳定决胜条件。该策略当前属于内部默认行为，
未来公共 renderer 描述可以增加设备 ID 或功耗偏好，而不暴露 `VkPhysicalDevice`。

公开 renderer registry 使用互斥锁保护句柄表，并以共享所有权保存内部状态。销毁先从 registry
移除句柄，再在锁外释放 Vulkan device；已取得状态的并发内部操作可以结束，新操作无法继续取得
已销毁句柄。每个 renderer 分配非零 domain，供后续子资源校验归属。

平台窗口或 surface 所需的原生信息通过独立的平台描述结构传入。未来如需支持原生 Vulkan
互操作，应放入明确标记的不稳定高级接口，不得污染基础 API。

GPU 资源内存计划由内部 Vulkan Memory Allocator（VMA）负责选择 Memory Type 和大块子分配。
公共 API 只表达 automatic、device、upload 和 readback 等访问意图，不暴露 VMA 类型、Vulkan
memory property 或 heap index。VMA 不进入安装导出，具体接入计划见
[R-01 GPU 内存分配方案](../plans/R-01-memory-allocation.md)。

资源公共模型区分 Buffer、Texture、Texture View 和 Sampler，并采用“完整模型、最小实现范围”。
第一阶段优先实现 Buffer、2D Texture、默认 View 和基础 Sampler，详细边界见
[R-02 第一版资源模型](../plans/R-02-resource-model.md)。

## C ABI 规则

- C 头文件必须能够由 C11 编译器独立包含。
- ABI 中只使用定宽整数、显式布局的结构体、函数指针和不透明整数句柄。
- 不跨动态库边界传递 STL 类型、C++ 对象、异常或所有权不明的内存。
- 错误通过结果码返回；详细诊断由独立查询或日志回调提供。
- 可扩展描述结构包含 `struct_size`，新增字段只允许追加到末尾。
- 数组和字符串使用“指针 + 长度”，并明确数据的借用期限。
- 回调必须说明调用线程、可重入性和用户数据生命周期。

操作结果使用定宽的 `int32_t`，而不是宽度由编译器决定的 C 枚举。零表示成功，负值表示失败；
具体错误由 `GRANIT_ERROR_*` 常量表示。`granit_result_message` 返回由库持有的静态英文文本，
仅用于诊断，不应作为程序逻辑或本地化界面依据。

Vulkan Validation Layer 与 Granit 生命周期验证职责分离：前者检查 Vulkan API、同步和 Layout，
后者检查公开句柄、Renderer domain、资源所有权和用户遗漏销毁。发现活动子资源时仍必须完成
Renderer 级联清理，不能通过销毁失败制造新的泄漏。

## 资源句柄

具有身份和生命周期的资源使用 64 位整数句柄，例如 renderer、surface、buffer、texture、
command recorder、shader、pipeline、swapchain 和 fence。零值统一表示无效句柄。

公共 ABI 只承诺句柄为 `uint64_t` 和零值无效，不公开或保证内部位布局。使用者不得解析、修改、
持久化或跨进程传递句柄。

句柄内部计划至少编码槽位索引和 generation，并由资源表记录资源类型与所属 renderer/device。
每次使用均验证：

- 句柄是否存在且 generation 匹配。
- 资源类型是否与当前操作匹配。
- 资源是否属于正确的 renderer/device。

资源销毁后递增 generation，防止旧句柄错误访问复用后的槽位。句柄只在当前进程和当前动态库
生命周期内有效，不可持久化，也不保证在动态库重新加载后继续有效。

当前内部句柄表使用低 32 位槽位索引、中间 24 位 generation 和高 8 位资源类型，并在槽位中
另外记录所属 domain。该布局属于实现细节，可以在不改变公共 ABI 的情况下调整。句柄表不拥有
资源对象，也不在内部提供并发访问保证；未来 renderer/device 必须在外层协调查找与销毁，确保
返回地址使用期间资源不会被并发释放。

颜色、范围、尺寸、viewport 和资源创建参数属于值数据，使用普通结构体而不是句柄。

## 性能边界

整数句柄会引入资源表查找，因此 API 应围绕具有实际语义的渲染操作设计。高频命令优先批量
记录和提交，避免将每个细粒度状态变更设计为一次独立的动态库调用。
