<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Vulkan 后端

## 定位

Vulkan 是桌面构建的私有 Renderer 后端。公共头、句柄、描述和结果码不暴露 `Vk*` 类型；Registry
通过 `src/backend/contracts` 中按职责拆分的 HAL 调用 Vulkan 实现。应用可以只使用 Granit 的
Renderer、Window 或第三方窗口 Integration，不需要包含 Vulkan SDK 头文件。

Vulkan 后端当前覆盖资源、Shader、Pipeline、命令、传输、Timestamp、提交、延迟回收和窗口呈现
完整路径。浏览器 WebGPU 使用同一公共 API 和 Registry，但由独立后端实现，二者不共享原生对象。

## Loader、Instance 与设备

Granit 使用锁定且版本匹配的 Vulkan-Headers 与 Volk。Volk 作为私有 object library 编入 Granit，
启用 `VK_NO_PROTOTYPES`；运行时加载系统 Vulkan loader，不向使用者传播 include 或链接依赖。

Loader 初始化结果在进程内缓存。每个 Instance 与 Device 分别保存独立的 `VolkInstanceTable` 和
`VolkDeviceTable`，不使用全局 instance/device 分发表，因此多个 Renderer 可以并存。

设备必须支持 Vulkan 1.3、graphics queue、dynamic rendering、synchronization2 和 maintenance4。
默认按独立显卡、集成显卡、虚拟显卡、CPU 和其他设备排序，同类型优先 device-local 内存更大的
设备，最后保留枚举顺序作为确定性决胜条件。

Renderer 创建描述只表达是否需要呈现。启用呈现时，Windows 构建启用 Win32 WSI，Linux 构建启用
已编译的 XCB/Wayland WSI；Surface 创建时再校验具体来源与 Queue 支持。纯离屏 Renderer 不要求
Surface 或 Swapchain 能力。

## HAL 与实现组织

`vulkan_renderer_state` 实现资源、Shader、Pipeline、命令、提交、呈现、Timestamp、调试名称和
回收等 HAL 职责。声明集中在 `src/backend/vulkan/renderer_state.h`，实现按领域拆分到
`renderer_state_*.cpp`，避免单一实现文件同时承担所有后端操作。

Registry 保存不可变的 `backend_interfaces` 能力快照。公共入口先校验句柄类型、generation、
Renderer domain 和跨资源关系，再调用对应后端职责；Vulkan 后端负责原生描述转换、同步、资源
状态和对象生命周期，不复制公共句柄表。

## 资源与内存

Buffer 与 Texture 使用私有 VMA allocator 分配。公共 memory location 只表达 device、upload、
readback 等访问意图；VMA 类型、Memory Type 和 Heap 选择不进入 ABI。

Texture View、Sampler、Shader、Bind Group、Pipeline 与 Timestamp Pool 各自拥有后端资源对象。
Swapchain Backbuffer 作为借用 Texture/View 发布到公共 Registry，不能通过普通资源销毁入口释放。

同步写入和读取使用内部 staging/upload/readback context；高频路径使用 Upload Batch、Readback
Batch 或 Command Recorder。格式能力、压缩块布局、多采样、Mipmap 和复制限制由公共层与后端
能力共同校验。

## 命令、提交与同步

Command Recorder 在 Vulkan 内部拥有 Command Pool 与 Command Buffer。Dynamic Rendering、资源
复制、Mipmap、Timestamp、Draw 和 Dispatch 都先记录，结束后由统一提交路径排序。

资源状态跟踪在提交顺序确定后生成 Buffer Barrier 与 Image Layout 转换。Frame Context 为每个在途
槽维护 Fence、acquire Semaphore、present Semaphore 和 Recorder 上下文；Frame 令牌关联实际槽、
Swapchain 图像和提交状态。

普通 GPU 资源销毁使用提交序号进入延迟回收队列，GPU 完成后才释放原生对象。Renderer 关闭会先
阻止新操作，等待全部提交完成，收集退役资源，再销毁帧上下文、Allocator、Device 和 Instance。

## Surface 与 Swapchain

使用 Granit Window 时，`granit_window_create_surface` 在 Window component 内取得平台值，普通应用
无需访问原生句柄。SDL3、GLFW、Qt 或引擎平台层可以显式包含 `native_surface.h`，通过带标签的
Win32、XCB 或 Wayland 描述创建同一种公共 Surface。

Swapchain 负责能力查询、格式和 Present Mode 选择、Backbuffer 发布、重建、acquire、cancel 与
present。重建和关闭等低频 WSI 路径会等待相关 Queue 安全完成；普通资源回收不使用全局
`vkDeviceWaitIdle`。

Surface Lost、Out of Date、Suboptimal 与 Device Lost 映射为公共结果和状态，不把 `VkResult`
暴露给调用方。

## 诊断与验证

请求 Validation 时，后端检查并启用 `VK_LAYER_KHRONOS_validation` 与 debug utils。消息进入统一
Renderer diagnostic sink；公共回调未设置时由默认 sink 输出。GPU 调试名称也通过可选 HAL 职责
写入 Vulkan 对象。

纯逻辑测试覆盖结果映射、能力筛选、描述转换和生命周期；GPU 集成测试覆盖资源、命令、提交、
回读、离屏渲染与窗口帧循环。Windows 验证 Win32，Linux 在图形会话中验证 XCB 与 Wayland；缺少
Vulkan 1.3 loader、合适设备或显示服务器时，环境测试应明确跳过而不能伪装成功。

公共行为以对应 [Renderer Reference](../reference/renderer.md) 和各资源 Reference 为准；本文只说明
Vulkan 私有实现如何满足这些契约。
