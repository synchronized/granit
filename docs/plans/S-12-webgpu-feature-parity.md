<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-12：WebGPU 公共能力补齐

## 状态

- 实现状态：已确认，待开始
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
- 创建成功后通过可扩展的 Renderer Info 查询实际后端；不把后端信息塞入生命周期 Status，也不
  暴露 Provider、Adapter、Device 或原生句柄。
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
  uint32_t reserved[2];
} granit_renderer_info;
```

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

- 实验性 Provider ABI 直接升级版本，操作表尾部追加三个创建函数；不保留旧版兼容分支。
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

## 实施顺序

1. **S-12A 后端选择**：实现 C ABI、C++ 包装、实际后端查询、插件定位和严格/自动选择语义。
2. **S-12B 桌面呈现**：为 WebGPU Provider 接通 Win32、XCB 与 Wayland Surface；SDL3 继续通过
   对应原生窗口描述创建 Surface，不增加 SDL 专用 Renderer API。
3. **S-12C 几何资源**：接通 WebGPU Vertex/Index Buffer、顶点布局、索引格式和 Indexed Draw。
4. **S-12D 材质资源**：接通 Texture、Texture View、Sampler、Bind Group 与 PBR 所需基础格式。
5. **S-12E 每帧数据**：将动态 Uniform Offset、对齐限制和逐帧上传路径映射到两个后端。
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
- 后端不可用、能力不足、资源跨 Renderer 混用和动态偏移错误均有确定结果码。
- 同一确定性 Fixture 在 Vulkan、桌面 Dawn WebGPU 和 Emscripten WebGPU 产生容差内一致的结果。
- Windows、Linux 和 Emscripten 手动 Actions 通过；安装包不泄漏任何后端实现依赖。
- 完成后，S-13 不需要使用后端条件编译即可创建模型查看器所需 GPU 资源。

## 风险与未决问题

- Provider 默认安装位置需要同时覆盖构建树、安装树和应用打包；不得依赖不安全的隐式动态库搜索。
- WebGPU 格式、映射和提交完成语义与 Vulkan 不完全对等，需要能力查询与明确降级。
- 浏览器下载、解码和文件系统不属于 Renderer；S-13 应保持资源来源与渲染后端分离。
