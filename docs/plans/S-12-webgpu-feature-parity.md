<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-12：WebGPU 公共能力补齐

## 状态

- 实现状态：进行中；S-12A 至 S-12E 已完成，下一步 S-12F
- 前置依赖：S-10
- 后续任务：S-13
- 优先级：P1

## 背景与目标

S-10 已打通 Emscripten WebGPU 的公共 Renderer、Canvas 和无顶点输入三角形闭环，但普通桌面
应用还不能通过公共 API 选择 WebGPU，浏览器路径也不足以渲染带顶点、索引、纹理和动态 Uniform
的模型。S-12 负责补齐 S-13 模型查看器所需的最小跨后端能力，而不是追求 Vulkan 与 WebGPU 的
全部功能完全对等。

目标包括：

- 提供后端无关的 Renderer 选择与实际后端查询，不暴露 Vulkan、Dawn 或 WebGPU 原生类型。
- 让 Vulkan、桌面 Dawn WebGPU 与 Emscripten WebGPU 共用 Buffer、Texture、Sampler、Bind Group、
  Graphics Pipeline、动态 Uniform、Indexed Draw 和上传路径。
- 以同一组公共 API 测试验证资源所有权、错误语义、能力降级和绘制结果。

## 非目标

- 不在本任务实现 glTF、场景编辑器、资产缓存或模型格式。
- 不要求 WebGPU 支持 Vulkan 的全部格式、同步、查询、Bindless 或高级渲染能力。
- 不增加公开的 Vulkan/WebGPU 原生互操作接口。
- 不把 Dawn、Emscripten 或 WebGPU 头文件传播给 Granit 使用者。

## 已确认决策

- 在可扩展 Renderer 创建描述尾部增加后端偏好；使用定宽枚举表达 `auto`、`vulkan` 和
  `webgpu`。旧 `struct_size` 与 `GRANIT_RENDERER_DESC_INIT` 均保持 `auto`，不破坏现有调用方。
- 显式选择 `vulkan` 或 `webgpu` 时严格执行，不自动回退；这保证测试、诊断和性能数据不会因环境
  变化而悄悄切换后端。
- `auto` 在桌面优先 Vulkan，仅当候选后端不可用、不兼容、无合适设备或不支持请求的 Surface 时
  尝试 WebGPU；Emscripten 只选择静态 WebGPU。内存不足、无效参数和 Granit 内部错误不触发回退。
- 创建成功后通过可扩展的 Renderer Info 查询实际后端与稳定的只读 Adapter 元数据；
  不把这些信息塞入生命周期 Status，也不暴露 Provider、Adapter、Device 对象或原生句柄。
- 桌面 WebGPU 使用现有 Granit Provider 插件。创建描述允许传入可选插件绝对路径；空路径使用
  Granit 定义的安装/构建产物位置，不扫描当前工作目录或任意系统 `PATH`。Emscripten 静态接入时
  路径必须为空。
- 插件路径在 `granit_renderer_create` 调用期间复制或消费，调用返回后不借用调用者字符串。
- 不复制公共 Registry；具体能力继续通过现有私有 HAL 和后端工厂实现。
- 不用“静默不绘制”表达能力不足；不支持的组合返回稳定结果码，并由能力查询提前暴露限制。
- 高频绘制继续使用命令记录和批量提交，不为每个顶点、纹理或 Uniform 更新增加独立动态库调用。

### S-12A 公共契约草案

以下代码只固定字段含义，最终命名和布局以 ABI 测试后的公共头文件为准：

```c
typedef uint32_t granit_renderer_backend;
#define GRANIT_RENDERER_BACKEND_AUTO UINT32_C(0)
#define GRANIT_RENDERER_BACKEND_VULKAN UINT32_C(1)
#define GRANIT_RENDERER_BACKEND_WEBGPU UINT32_C(2)

/* 追加到 granit_renderer_desc 尾部。 */
granit_renderer_backend backend;
uint32_t backend_library_path_length;
const char* backend_library_path;

typedef struct granit_renderer_info {
  uint32_t struct_size;
  granit_renderer_backend backend;
  char* adapter_name;
  uint32_t adapter_name_capacity;
  uint32_t adapter_name_length;
  uint32_t vendor_id;
  uint32_t device_id;
  uint32_t reserved[2];
} granit_renderer_info;
```

`adapter_name` 使用调用者缓冲区：空指针与零容量只查询所需 UTF-8 长度，容量包含
结尾零字符，`adapter_name_length` 不包含它。后端或浏览器隐私策略不提供名称时
返回空串，Vendor/Device ID 不可用时返回零；这些字段只用于诊断和性能记录，
不得作为渲染行为分支条件。

错误语义如下：

- 未知枚举、非空长度配空指针、路径包含内嵌零字符或静态平台传入路径：
  `GRANIT_ERROR_INVALID_ARGUMENT`。
- 插件文件或所选后端未安装：`GRANIT_ERROR_BACKEND_UNAVAILABLE`。
- 插件 ABI、类型或版本不匹配：`GRANIT_ERROR_INCOMPATIBLE_DRIVER`。
- 找不到满足要求的 Adapter/Device：`GRANIT_ERROR_NO_SUITABLE_DEVICE`。
- 后端存在但不支持请求的 Surface 或能力：`GRANIT_ERROR_UNSUPPORTED`。
- `auto` 所有候选均失败时返回优先候选最能说明原因的结果，并通过 Diagnostic Callback 报告每次
  尝试；显式选择只报告所选后端的结果。

### S-12B 桌面 Surface 契约

公共 `granit_surface_create_win32`、`granit_surface_create_xcb` 和
`granit_surface_create_wayland` 保持不变。SDL3 Integration 继续把 `SDL_Window` 转换成这些公共
描述，不感知 Renderer 实际使用 Vulkan 还是 WebGPU。

WebGPU Provider 插件 ABI 增加与公共入口对应的三组描述和创建函数，不使用一个携带无类型字段的
通用结构体：

```c
typedef struct granit_backend_plugin_win32_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* instance;
  void* window;
} granit_backend_plugin_win32_surface_desc;

typedef struct granit_backend_plugin_xcb_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* connection;
  uint32_t window;
  uint32_t reserved_2;
} granit_backend_plugin_xcb_surface_desc;

typedef struct granit_backend_plugin_wayland_surface_desc {
  uint32_t struct_size;
  uint32_t reserved;
  void* display;
  void* surface;
} granit_backend_plugin_wayland_surface_desc;
```

- 实验性 Provider ABI 已直接升级到 v8，操作表追加三个创建函数；不保留旧版兼容分支。
- 锁定 Dawn 头已经提供 Windows HWND、XCB Window 和 Wayland Surface 链结构，Provider 内部负责
  转换，核心与公共头不包含 Dawn 或平台 SDK 类型。
- 创建调用期间只借用描述结构和指针值；Granit 不拥有原生窗口、Display、Connection 或 Surface。
  调用者必须让对应原生对象至少存活到 Granit Surface 销毁，且不得在其仍被使用时从其他线程销毁。
- Renderer 创建描述中的 `surface_types` 继续作为允许集合。创建未声明类型返回
  `GRANIT_ERROR_UNSUPPORTED`，无效空句柄返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- Provider 能力快照尾部增加 `surface_types` 位集；`auto` 在创建 Renderer 前用请求位集筛选候选，
  实际创建 Surface 时仍由 Provider 再校验平台与设备支持。
- 一个 Surface 同时只能属于一个 Renderer；跨 Renderer、错误类型、重复销毁和父 Renderer 销毁后
  使用继续由统一 generation 句柄表拒绝。

呈现和重建沿用现有公共 Swapchain/Frame 语义：

- 首次配置及窗口尺寸变化都通过 `granit_swapchain_create/recreate`，不增加 WebGPU 专用入口。
- Native WebGPU 在成功提交帧后执行显式 Surface Present；Emscripten 仍由浏览器隐式呈现，不调用
  平台禁止的显式 Present。
- Acquire 返回的 Backbuffer Texture/View 继续由 Swapchain 借出，在 Present、Cancel、重建、
  Surface Lost 或销毁后立即失效。
- 零尺寸窗口返回 `GRANIT_ERROR_NOT_READY`；过期配置返回 `GRANIT_ERROR_OUT_OF_DATE`，Surface 丢失
  返回 `GRANIT_ERROR_SURFACE_LOST`。调用方据此等待非零尺寸、重建 Swapchain 或重建 Surface。
- Registry 串行化同一 Renderer 的 Surface/Swapchain 操作，但不替调用方同步窗口系统事件线程。

### S-12C 几何与绘制契约

S-12C 已完成。Provider ABI v13 已接通 Vertex/Index Buffer、顶点布局、显式 Render Pass 命令、
普通与索引绘制；旧固定三角形 `recorder_draw` 和 Registry 的 WebGPU 专用命令状态均已删除。

现有公共 API 已表达 Vertex/Index Buffer usage、多个 Vertex Buffer Layout、Attribute、逐顶点或
逐实例步进、Buffer 绑定以及 Indexed Draw。S-12C 不新增平行公共接口，只扩展私有 HAL 与 WebGPU
Provider 实现这些现有语义。

首轮模型查看器需要的共同子集如下：

- Buffer usage 支持 `VERTEX`、`INDEX`、`TRANSFER_SOURCE` 和 `TRANSFER_DESTINATION` 的合法组合。
- Index 类型支持 `uint16` 与 `uint32`，不增加 WebGPU/Vulkan 均非通用的 `uint8` 索引。
- Vertex Attribute 至少支持 `float32`、`float32x2`、`float32x3` 和 `float32x4`；足以表达位置、
  法线、切线、UV 和实例变换。整数格式继续按现有公共枚举支持，不为 glTF 新增专用格式。
- 支持多个 Vertex Buffer binding、非零 binding offset、逐顶点/逐实例步进、`first_vertex`、
  `first_index`、有符号 `vertex_offset`、`first_instance` 和多实例绘制。
- 首轮只要求 Triangle List 模型闭环；其他已公开拓扑由能力与 Provider 验证决定，不静默替换。

Provider ABI 需要完成以下演进：

1. Buffer usage 增加 Vertex 与 Index 位，并在 Provider Buffer 记录中保留创建大小和 usage。
2. Graphics Pipeline 描述增加 Vertex Buffer Layout 与 Attribute 数组；调用期间深拷贝或同步消费，
   不借用 Host 数组到调用返回之后。
3. Command 操作表增加 Begin/End Rendering、Bind Graphics Pipeline、Bind Vertex Buffers、
   Bind Index Buffer、Draw 与 Draw Indexed；参数使用定宽整数和插件资源句柄。
4. 删除只接受 Target/Pipeline/Bind Group 并固定执行三顶点绘制的 `recorder_draw` 语义。实验性插件
   ABI 直接升级，不同时保留新旧命令分支。
5. WebGPU Renderer 移除 `platform_managed_rendering` 固定三角形捷径，改由 Command Adapter 实现与
   Vulkan 相同的公共记录顺序：Begin Rendering、Bind、Draw、End Rendering、Finish、Submit。

验证规则由 Registry 先执行，Provider 仍须防御不可信插件调用：

- Vertex binding 数、Attribute 数、最大 location、stride 与 offset 受公开能力限制；Attribute 范围
  必须落在 stride 内，location 不得重复，所有加法和乘法均做溢出检查。
- Vertex Buffer offset 必须小于 Buffer 大小；Index Buffer offset 必须在范围内并满足 2 或 4 字节
  对齐。Draw Indexed 的索引读取范围不得越过绑定 Buffer。
- Buffer 必须带对应 usage 并属于同一 Renderer；空数组、首 binding 加数量溢出、无 Pipeline、
  无 Index Buffer 或在 Render Pass 外 Draw 均返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- 命令成功记录后由 Recorder 保留 Pipeline 与 Buffer 的内部共享所有权；用户可在 Submit 前销毁
  公共句柄，但不得影响已记录命令。失败记录不改变命令状态或资源保留集合。
- Provider 限制不足返回 `GRANIT_ERROR_UNSUPPORTED`，无效公共参数返回
  `GRANIT_ERROR_INVALID_ARGUMENT`，错误归属或失效句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。

### S-12D 材质资源契约

S-12D 复用现有 Texture、Texture View、Sampler 和 Bind Group 公共 API，不引入 PBR 或
glTF 专用资源接口。WebGPU Provider 必须从当前固定“一张 RGBA8 Texture 加一个
Sampler”的描述升级为数组化、由 Layout 驱动的通用资源绑定。

首轮模型查看器需要的共同子集如下：

- Texture 支持 2D、单采样、单数组层和多 mip；首轮必须支持 `R8_UNORM`、
  `RG8_UNORM`、`RGBA8_UNORM`、`RGBA8_SRGB` 和 `D32_FLOAT`。其他公开格式在 Provider
  未实现时明确返回 `GRANIT_ERROR_UNSUPPORTED`。
- Base Color 与 Emissive 使用 sRGB 格式；Normal、Metallic-Roughness 和 Occlusion 使用
  线性格式。后端依赖格式解码，Shader 不重复执行 sRGB 转换。
- `granit_texture_write` 可写入指定 mip 和矩形区域，处理紧密或显式行跨度；Host 负责
  WebGPU 行对齐与暂存复制，不将平台对齐规则泄漏到公共 API。
- 首轮不要求 WebGPU 运行时生成 mip；模型资源路径应上传预生成或 CPU 生成的
  完整 mip 链。不得在启用 mip 采样时静默退化为单层。
- Sampler 支持现有 min/mag/mipmap Filter、U/V/W Address Mode 与 LOD 范围。各向异性和
  Compare Sampler 依能力支持；不支持时不改写请求。
- Bind Group Layout 保留任意 binding 号、可见阶段和资源类型；Bind Group 按 Layout
  数组化创建并支持 Uniform Buffer、Sampled Texture 与 Sampler 混合。首轮仅保证
  `array_count = 1`，资源数组留给后续能力任务。
- Pipeline Layout 支持多个 Bind Group Layout；材质常量、五类 PBR Texture 和共享
  Sampler 只是一个测试用例，不固化到 Provider ABI。

Registry 在进入 Provider 前校验重复或缺失 binding、资源类型、usage、Buffer 范围、
Texture View 子资源和 Renderer 归属。Bind Group 保留 Layout 与所有绑定资源的内部
共享所有权。测试覆盖线性/sRGB 组合、多 mip 上传、采样状态、多 Bind Group、
类型错误、缺失项、跨 Renderer 句柄与不支持数组。Provider ABI 直接升级，不保留
旧的固定 Texture/Sampler 分支。

S-12D 已完成：Provider ABI v21 接收显式格式、mip 数量、
View mip 范围、写入区域、Sampler 状态和由 Layout 驱动的绑定数组，
公共 Renderer 已接通上述五种必需格式的二维、单层、单采样 Texture 创建、子 View 创建，以及
指定 mip、矩形区域和显式行跨度上传。Sampler 已接通过滤、寻址、LOD 范围和整数倍
各向异性；WebGPU 没有 LOD Bias 描述字段，非零请求明确返回 `GRANIT_ERROR_UNSUPPORTED`。
Bind Group 已支持任意 binding 号以及 Uniform、Dynamic Uniform、Storage Buffer、Sampled Texture
和 Sampler 混合，首轮仍限制 `array_count = 1`。比较 Sampler 需要公共 Layout 增加绑定子类型，
当前创建时明确返回 `GRANIT_ERROR_UNSUPPORTED`，不生成与 Layout 不匹配的 WebGPU 对象。
Pipeline Layout 已支持空布局和多个 Bind Group Layout，并在 Provider 内保留其依赖关系，防止布局
仍被 Pipeline Layout 使用时提前销毁。比较 Sampler 的公共契约扩展留给后续独立能力任务。

### S-12E 每帧数据契约

S-12E 不新增 Uniform Arena 公共类型；上层继续用 Buffer、Upload Batch、Renderer Limits
和现有动态 Offset 数组组合逐帧策略。后端共同契约如下：

- WebGPU Provider 将 `DYNAMIC_UNIFORM_BUFFER` 映射为带动态 Offset 的 Uniform Binding，
  Graphics 与 Compute Bind Group 均按 Layout 中 binding 升序消费 Offset。
- `granit_renderer_get_limits` 返回实后端的 Uniform Offset 对齐与最大绑定大小，
  不使用固定 256 字节假设。
- Registry 校验 Offset 数量、对齐、基础 Offset 加动态 Offset 加 Range 的溢出与
  Buffer 越界；Provider 保留等价防御性校验。
- Upload Batch 将同一批 Buffer/Texture 写入合并为一次 Provider 调用；`submit`
  成功后才释放 Host 持有的数据副本，单次 `buffer_write` 仍保留便利语义。
- 浏览器不暴露持久映射指针；不可直接映射的 Upload Buffer 使用 Host 副本和
  Queue Write 实现，公共 Map 请求明确返回 `GRANIT_ERROR_UNSUPPORTED`。

验证覆盖单个 Uniform Buffer 内两组变换、多 Bind Group、Graphics/Compute、数量错误、
未对齐、越界、整数溢出与批量上传失败原子性。Vulkan 与 WebGPU 复用同一组
Registry 契约测试，Provider Mock 只验证后端参数映射。

当前已完成图形命令的动态 Uniform Offset 路径：公共 Registry 校验后，WebGPU HAL 将多个
Bind Group 与动态偏移批量传给 Provider；Provider 按 Pipeline Layout 中的组顺序及 Layout 中
递增的 binding 顺序切分偏移，并再次校验数量、设备对齐、Buffer 范围、句柄归属和录制状态。
Upload Batch 已支持在同一批次混合 Buffer 与二维单层 Texture 写入，并通过一次 Provider ABI 调用
提交；Provider 在执行任何 Queue Write 前完整校验整批描述，公共 Registry 只在成功后统一释放 Host
副本。WebGPU Compute Pipeline、惰性 Compute Pass、Compute Bind Group 动态偏移和 Dispatch
也已接通；Provider 会再次校验偏移数量、顺序、设备对齐和 Buffer 范围。S-12E 已完成。

### S-12F 跨后端 Fixture 契约

Fixture 是不依赖窗口的 64×64 离屏测试，不使用 S-13 头盔模型或外部资产。它由
固定种子生成带 UV/法线的索引几何、sRGB Base Color、线性 Normal 与
Metallic-Roughness Texture，并用同一 Uniform Buffer 的两个动态 Offset 绘制两个实例。

- Vulkan、桌面 Dawn 与 Emscripten 调用同一 Fixture 函数；只在 Renderer 创建参数中选择
  后端，测试正文不含后端条件分支。
- Shader Asset 包同一逻辑的 Vulkan 与 WebGPU 变体；测试不在运行时调用 Tint。
- 回读统一为 RGBA8 线性比较；检查背景、两个实例与遮挡区域的语义探针，
  有效像素通道绝对误差不超过 2/255。不使用跨 GPU 不稳定的整图精确哈希。
- 失败时保存实际图、期望图、差异图及后端/适配器元数据；成功时不上传产物。

### S-12G 验收门槛

- Registry 契约测试在无 GPU 环境全部通过，WebGPU Provider Mock 覆盖全部新 ABI 操作。
- Windows 手动 Actions 运行 MSVC/Clang、Vulkan Fixture 与 Dawn Fixture；Linux 运行
  GCC/Clang、Vulkan Fixture 与 Dawn Fixture；Emscripten 在 Chromium WebGPU 中运行同一 Fixture。
- 共享/静态安装 Consumer、C11/C++20 公共头、ABI 布局、插件版本拒绝和安装包
  不泄漏 Dawn/Vulkan 依赖全部通过，才可将 S-12 标记完成并启动 S-13。

## 实施顺序

1. **S-12A 后端选择（已完成）**：已实现 C ABI、C++ 包装、实际后端查询、固定位置插件定位、
   严格选择与桌面 Vulkan 优先的自动回退；旧描述兼容、动态 Provider、失败诊断和 Emscripten
   静态选择均已验证。
2. **S-12B 桌面呈现（已完成）**：为 WebGPU Provider 接通 Win32、XCB 与 Wayland Surface；
   SDL3 继续通过对应原生窗口描述创建 Surface，不增加 SDL 专用 Renderer API。
3. **S-12C 几何资源（已完成）**：已接通 WebGPU Vertex/Index Buffer、顶点布局、索引格式、
   显式 Render Pass 命令和 Indexed Draw。
4. **S-12D 材质资源（已完成）**：已接通 Texture、子 View、区域上传、Sampler、通用
   Bind Group，以及空布局和多 Bind Group Pipeline Layout。
5. **S-12E 每帧数据（已完成）**：已将动态 Uniform Offset、对齐限制、混合 Upload Batch 和
   Compute 命令路径映射到两个后端。
6. **S-12F 跨后端 Fixture**：同一带纹理索引 Mesh 在 Vulkan、桌面 WebGPU 和浏览器 WebGPU 绘制。
7. **S-12G 验收**：验证公共头、共享/静态安装 Consumer、错误路径、截图结果和平台矩阵。

## 测试与验收

- C11 与 C++20 Consumer 能创建指定后端并查询实际后端。
- 旧版创建描述在桌面仍选择 Vulkan，在 Emscripten 仍选择静态 WebGPU；新增尾字段不会读取超出
  调用者 `struct_size` 的内存。
- 显式选择从不回退；`auto` 的候选顺序、可回退结果码及逐次诊断均由测试固定。
- 桌面 WebGPU 能通过 Granit Surface/Swapchain 公共 API 在 Win32、XCB 与 Wayland 窗口呈现。
- Mock Provider 覆盖三种描述布局、空值、允许位集、归属、销毁顺序、Surface Lost 和重建；真实
  Dawn Smoke 在 Windows HWND、Linux XCB 与 Wayland 环境各完成多帧 Acquire/Submit/Present。
- SDL3 + ImGui Smoke 不包含后端条件分支；测试参数只改变 Renderer 后端选择，并验证窗口缩放、
  最小化恢复和退出期间的资源清理。
- 几何测试覆盖单一交错 Vertex Buffer、多个 Buffer、实例步进、`uint16/uint32` Index、非零 Offset、
  Base Vertex 和 First Instance，并在两个后端验证相同像素与资源生命周期。
- 几何失败测试覆盖 usage 错误、错位 Index、越界 Offset/Index 范围、重复 location、Attribute 越过
  stride、超过设备限制、跨 Renderer 资源及错误命令顺序。
- 后端不可用、能力不足、资源跨 Renderer 混用和动态偏移错误均有确定结果码。
- 同一确定性 Fixture 在 Vulkan、桌面 Dawn WebGPU 和 Emscripten WebGPU 产生容差内一致的结果。
- Windows、Linux 和 Emscripten 手动 Actions 通过；安装包不泄漏任何后端实现依赖。
- 完成后，S-13 不需要使用后端条件编译即可创建模型查看器所需 GPU 资源。

## 风险与未决问题

- Provider 默认安装位置需要同时覆盖构建树、安装树和应用打包；不得依赖不安全的隐式动态库搜索。
- WebGPU 格式、映射和提交完成语义与 Vulkan 不完全对等，需要能力查询与明确降级。
- 浏览器下载、解码和文件系统不属于 Renderer；S-13 应保持资源来源与渲染后端分离。
