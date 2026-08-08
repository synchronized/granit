<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 架构与 ABI

## 产品定位

Granit 定位为基于 Vulkan 的中层、显式、可嵌入式渲染库，主要服务自研游戏引擎、实时应用和
图形工具。它隐藏 Vulkan 的实例、设备、同步和资源管理细节，但保留现代图形 API 中明确的资源
用途、命令记录、提交和生命周期语义。

核心库采用“Bring Your Own Engine”边界，不直接拥有使用者的 Scene、Entity、Camera、Light、
动画或资产数据库。PBR、场景渲染、后处理套件和 Render Graph 可以在核心能力稳定后作为独立
高层模块提供，不能反向污染稳定 C ABI。

当前只实现 Vulkan 后端，不承诺同时支持 Direct3D、Metal 或 OpenGL。公共概念仍保持后端中立，
目的是避免 Vulkan 实现细节泄漏，而不是立即承担多后端的最低公共能力限制。

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

## 分层

Granit 使用三层接口隔离使用者与 Vulkan：

1. `.hpp` C++20 API：面向普通用户，提供强类型、移动语义和 RAII。
2. `.h` C API：动态库的稳定 ABI，也是其他语言绑定的基础。
3. 内部实现：资源表、渲染调度和 Vulkan 后端，不进入公共头文件。

C++ 包装层保持轻量，不维护一套独立的渲染状态。它拥有或引用 C 句柄，并将所有实际操作转发给
C API。普通 C++ 用户不需要直接使用 C API。

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
[R-01 GPU 内存分配方案](plans/R-01-memory-allocation.md)。

资源公共模型区分 Buffer、Texture、Texture View 和 Sampler，并采用“完整模型、最小实现范围”。
第一阶段优先实现 Buffer、2D Texture、默认 View 和基础 Sampler，详细边界见
[R-02 第一版资源模型](plans/R-02-resource-model.md)。

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

## 资源句柄

具有身份和生命周期的资源使用 64 位整数句柄，例如 renderer、surface、buffer、texture、shader、
pipeline、swapchain 和 fence。零值统一表示无效句柄。

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
