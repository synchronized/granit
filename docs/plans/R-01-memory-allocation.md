<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-01：GPU 内存分配方案

## 元数据

- 设计状态：已确认
- 实现状态：待开始
- 路线图任务：R-01
- 优先级：P0
- 前置依赖：已创建 Vulkan Instance、Physical Device 和 Device
- 后续依赖：R-02、R-03、R-04、R-05、R-08

## 背景

Vulkan 要求应用选择 Memory Heap/Type、分配 `VkDeviceMemory`、处理对齐与 Buffer/Image
granularity，并显式管理非一致内存的 flush/invalidate。为每个资源单独分配会增加驱动开销，
还会受到 `maxMemoryAllocationCount` 限制，因此 Granit 需要大块内存子分配方案。

该能力属于 Vulkan 后端实现细节。普通用户只需要表达资源如何被 CPU 和 GPU 使用，不应选择
Vulkan memory type、property flag、heap index 或 dedicated allocation。

## 目标

- 为 Buffer 和 Texture 提供稳定、跨设备的内存类型选择与子分配。
- 同时适配独立显存 GPU、ReBAR 和 UMA/共享内存设备。
- 支持持久映射、非一致内存刷新、专用分配和显存预算。
- 保持 VMA、`VkDeviceMemory` 和内存类型索引完全位于 `src/` 内部。
- 为上传、回读、延迟销毁和后续特殊 Pool 提供基础。

## 非目标

- 本任务不实现 Buffer/Texture 公共资源 API，分别由 R-03 和 R-05 负责。
- 本任务不实现上传命令、Upload Ring 或 Queue 提交，由 R-04 和 F 系列任务负责。
- 本任务不解决 GPU 正在使用资源时的销毁安全，R-08 负责延迟销毁。
- 前期不实现稀疏资源、内存别名、主动 defragmentation 或显存分页策略。
- Vulkan 的 `VkAllocationCallbacks` 是驱动内部 CPU 分配回调，不与 GPU 资源分配混为一套接口。

## 已确认决策

### 使用 VMA

Granit 内部采用 Vulkan Memory Allocator（VMA），不自研通用 GPU 内存子分配器。主要原因：

- VMA 已处理 memory type 选择、块级子分配、对齐和 Buffer/Image granularity。
- 支持 dedicated allocation、持久映射、flush/invalidate 和 memory budget。
- 单头文件、MIT License，适合以内置源码形式参与可复现构建。
- VMA 不进入公共 ABI，未来仍可替换而不改变普通用户接口。

自研分配器不会提升 Granit 的核心差异化能力，却会显著扩大跨 GPU、跨驱动和多线程测试范围。

### 公共内存语义

公共接口使用 Granit 自己的定宽常量，初步确定四类：

```c
typedef uint32_t granit_memory_location;

#define GRANIT_MEMORY_LOCATION_AUTOMATIC UINT32_C(0)
#define GRANIT_MEMORY_LOCATION_DEVICE    UINT32_C(1)
#define GRANIT_MEMORY_LOCATION_UPLOAD    UINT32_C(2)
#define GRANIT_MEMORY_LOCATION_READBACK  UINT32_C(3)
```

- `AUTOMATIC`：根据资源用途选择，默认不承诺可映射。
- `DEVICE`：GPU 高频访问，通常位于 device-local 内存，不允许依赖直接映射。
- `UPLOAD`：CPU 写、GPU 读，允许映射并针对顺序写入优化。
- `READBACK`：GPU 写、CPU 读，在 CPU 读取前处理 invalidate。

这些值表达访问意图，不承诺具体 Heap、Memory Type 或物理内存位置。在 UMA 设备上，`DEVICE`
和 `UPLOAD` 可能落入同一种底层内存，但公共行为仍按声明语义执行。

### 默认 Pool 优先

前期使用 VMA 为各 Memory Type 管理的默认 Pool，让实际资源和测试数据决定特殊 Pool。以下能力
延后实现：

- 每帧 Upload Ring/线性分配器。
- Readback Pool。
- 瞬态 Texture/Attachment Pool。
- 大型资源专用 Pool。
- 资源别名和主动内存整理。

VMA 仍可根据资源要求自动选择 dedicated allocation；“不创建自定义 Pool”不代表禁止专用分配。

## 内部设计

预期所有权如下：

```text
renderer_state
├─ vulkan_instance
├─ vulkan_device
├─ vulkan_memory_allocator
│   └─ VmaAllocator
└─ resources
    ├─ buffer_state  → VkBuffer + VmaAllocation
    └─ texture_state → VkImage  + VmaAllocation
```

Allocator 在 Device 创建完成后初始化，在所有 Buffer、Texture 和延迟销毁项释放后、Device
销毁前释放。Swapchain 图像由 Vulkan 实现拥有，不创建 `VmaAllocation`。

建议增加内部 move-only `vulkan_memory_allocator`：

- 拥有 `VmaAllocator`，析构执行成对释放。
- 初始化接收 Instance、Physical Device、Device 和对应函数表。
- 提供创建/销毁 Buffer、Texture 的内部操作或为对应资源对象提供受控访问。
- 提供 Heap Budget 和统计查询，但不把 VMA 结构传出后端。
- 初始化失败映射为现有 Granit 结果码。

Buffer 和 Texture 应将原生对象与 `VmaAllocation` 放在同一内部 RAII 对象中，避免绑定成功后某一
步骤抛出或返回失败造成半初始化资源泄漏。

## VMA 接入

- 将经过验证的稳定标签版本锁定到 `3rd/`，在 `3rd/README.md` 记录来源、版本和 MIT License。
- VMA 只编译进 Granit 内部，不创建传播给使用者的公共依赖。
- 仅一个 `.cpp` 定义 `VMA_IMPLEMENTATION`。
- 禁用 VMA 静态和动态 Vulkan 函数查找，由 Granit 显式提供 Volk 已加载的函数指针，避免链接
  `vulkan-1` 或绕过现有函数表策略。
- 根据实际启用能力配置 Vulkan 版本、maintenance 和 memory budget 标志，不能假定扩展存在。
- VMA 头文件按第三方依赖处理，不应用 Granit 自有源码的警告即错误规则。

具体版本在实施开始时依据最新稳定标签、Vulkan-Headers 1.4.350 兼容性和双编译器验证后锁定。
版本选择完成前不得引用浮动分支或在线下载作为默认构建路径。

## 映射与缓存一致性

R-03 应基于以下规则设计 Buffer 映射 API：

- 只有满足公开内存语义的资源允许映射；不能因为某台 UMA 设备偶然可映射就放宽 `DEVICE` 契约。
- `UPLOAD` 映射用于 CPU 写入；非 host-coherent 内存在提交给 GPU 前必须 flush。
- `READBACK` 映射用于 CPU 读取；GPU 完成写入后、CPU 读取前必须 invalidate。
- 范围操作必须由内部实现按 `nonCoherentAtomSize` 对齐，不能要求普通用户理解 Vulkan 对齐。
- 是否内部持久映射不属于公共承诺；公开 map/unmap 表达访问周期，而非底层真实映射次数。
- 同一资源的嵌套映射、跨线程映射和销毁并发规则在 R-03 中明确，初版可以拒绝并发写映射。

初始数据传入 Buffer/Texture 创建描述时，指针只在调用期间借用。`DEVICE` 资源需要暂存上传时，
Granit 必须复制或记录数据所有权，不能在函数返回后继续读取调用者指针。

## 线程安全

- 每个 Renderer 拥有独立 Allocator，不在不同 Renderer 间共享 VMA 状态。
- 资源 Registry 负责句柄、domain 和公开生命周期；VMA 只负责内存，不替代资源所有权管理。
- 初版使用 VMA 默认内部同步，不启用要求 Granit 对全部分配调用进行外部同步的模式。
- 同一 Buffer/Texture 的 map、unmap、销毁和上传冲突由资源级状态或锁处理。
- 不在持有全局 Registry 锁时执行可能触发新 Device Memory Block 的耗时分配。

## 错误与诊断

- Host/Device 内存不足分别映射到 Granit 已有的 `GRANIT_ERROR_OUT_OF_MEMORY`；内部诊断可保留
  更具体来源，但公共控制流不依赖 VMA/Vulkan 数值。
- 找不到满足要求的内存类型返回 `GRANIT_ERROR_UNSUPPORTED` 或更具体的后续资源描述错误。
- 映射不允许的资源返回 `GRANIT_ERROR_UNSUPPORTED`，非法范围返回
  `GRANIT_ERROR_INVALID_ARGUMENT`。
- 后续提供每个 Heap 的 budget、usage、block bytes 和 allocation bytes 诊断查询。
- Debug 构建为分配记录资源类型、句柄和可选名称，便于泄漏定位。

## 实施步骤

1. 锁定 VMA 稳定版本，补充第三方来源、许可证和 CMake 内部目标。
2. 增加唯一 VMA 实现编译单元，并通过 Volk 函数指针初始化。
3. 实现 `vulkan_memory_allocator` RAII 和 Renderer 初始化/销毁顺序。
4. 增加最小内部测试资源，验证 Buffer/Image 分配、映射和释放。
5. 验证非拥有 Swapchain Image 不进入 VMA 销毁路径。
6. 在 R-02/R-03 中正式落地公共内存语义和 Buffer API。
7. 增加预算和泄漏诊断，再将计划状态改为已完成。

## 测试矩阵

- Windows Clang 动态库与 MSVC 静态库严格警告构建。
- Renderer 创建/销毁同时创建和销毁 VMA Allocator，不新增公共导出符号。
- Host-visible Buffer 分配、映射、写入、flush 和释放。
- Device-local Buffer 与 Texture 分配和释放。
- 非一致内存的范围对齐逻辑使用不依赖特定 GPU 的单元测试覆盖。
- 多线程创建/销毁独立资源的压力测试。
- 分配失败、无合适 Memory Type 和非法映射的错误映射测试。
- Renderer 级联销毁无泄漏，Swapchain 非拥有图像不会被错误释放。
- 若测试机只有 UMA 或只有 host-coherent 类型，必须保留可注入能力数据的确定性单元测试。

## 验收标准

- VMA 类型、宏、头文件和链接需求不出现在 `include/`、安装导出或使用者编译命令中。
- 普通用户只通过 Granit 内存语义表达访问意图。
- 所有资源内存在 Device 之前释放，错误路径不存在原生对象或 allocation 泄漏。
- 默认构建不直接链接系统 Vulkan Loader，继续使用现有 Volk 函数加载策略。
- 动态库导出表不因引入 VMA 增加第三方符号。
- 双编译器、动态/静态构建和相关测试全部通过。

## 未决问题

- 锁定的 VMA 具体稳定版本。
- Memory Budget 是创建 Renderer 的硬要求，还是可选诊断能力。
- 公开 map/unmap 是否自动处理全部 flush/invalidate，或额外提供显式 range API。
- `AUTOMATIC` 是否保留在第一版公共 API，还是只允许三种明确位置。
- 大型 Buffer/Texture 触发 dedicated allocation 的诊断阈值是否需要公开查询。

这些问题必须在 R-02 和 R-03 公共 API 定稿前关闭；不影响“内部采用 VMA”的总体决策。

## 参考资料

- [Khronos Vulkan Guide：Memory Allocation](https://docs.vulkan.org/guide/latest/memory_allocation.html)
- [Vulkan Memory Allocator 官方文档](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/)
- [VMA：Choosing Memory Type](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/choosing_memory_type.html)
- [VMA：Statistics and Budget](https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/statistics.html)

## 实现结果

尚未实现。完成后在此记录最终版本、关键差异、验证环境和相关提交。
