<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Vulkan 后端

## 当前状态

当前后端已经实现：

- 通过 Volk 线程安全地初始化进程内 Vulkan loader。
- 查询 loader 支持的最高 instance 版本，并要求至少支持 Vulkan 1.3。
- 将常用 `VkResult` 映射为后端无关的 `granit_result`。
- 创建和销毁无窗口 `VkInstance`。
- 为每个 instance 建立独立的 `VolkInstanceTable`。
- 可选检查并启用 Khronos validation layer 与 debug utils messenger。
- 枚举并筛选满足 Vulkan 1.3 基础要求的物理设备。
- 创建逻辑设备、graphics queue 和独立 `VolkDeviceTable`。
- 按需启用 Win32 Surface 扩展，并检查设备与队列的呈现能力。
- 创建、查询、重建和销毁 Win32 Surface 对应的 Swapchain。

GPU 内存分配基础已经通过内部 VMA 3.3.0 实现，包括 Renderer 级 Allocator、Buffer/Image
分配、持久映射以及 flush/invalidate。公共 Buffer/Texture API 和命令提交尚未实现。

## Loader 生命周期

Loader 状态通过函数局部静态对象初始化，依赖 C++ 的线程安全初始化保证。初始化结果在进程内缓存；
如果系统没有 Vulkan loader，返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`；loader 版本低于 Vulkan 1.3
时返回 `GRANIT_ERROR_INCOMPATIBLE_DRIVER`。

Granit 不调用全局 `volkLoadInstance` 或 `volkLoadDevice`。每个内部 instance 保存自己的
`VolkInstanceTable`，每个逻辑设备保存独立的 `VolkDeviceTable`，以支持多个 renderer/device
并存，避免全局函数指针被后创建的设备覆盖。

## Instance

`vulkan_instance` 是内部 move-only RAII 类型，请求 `VK_API_VERSION_1_3`。应用名称会在创建期间
复制并转换为零结尾字符串，不依赖调用者额外提供终止符。

验证层是可选能力。请求验证但缺少 `VK_LAYER_KHRONOS_validation` 或
`VK_EXT_debug_utils` 时返回 `GRANIT_ERROR_UNSUPPORTED`。当前调试回调将 warning 和 error 路由到
Renderer diagnostic sink；尚未设置公共回调时由 sink 输出到标准错误流。

## 设备选择

候选设备必须支持 Vulkan 1.3、graphics queue、dynamic rendering、synchronization2 和
maintenance4。不满足任一要求的设备不会进入排序。

默认选择顺序为独立显卡、集成显卡、虚拟显卡、CPU 实现和其他设备。同类型设备优先选择
device-local 显存更大的设备；仍然相同时保留 Vulkan 的原始枚举顺序，保证选择结果确定。

逻辑设备当前只创建一个 graphics queue，并启用上述三项 Vulkan 1.3 feature。每个设备持有独立
`VolkDeviceTable`。销毁设备前会调用 `vkDeviceWaitIdle`；后续建立显式提交和关闭流程后再评估
更细粒度的等待策略。

## 测试策略

结果映射和设备选择策略测试不依赖运行环境。Loader、instance 和 device 测试使用当前机器真实
Vulkan 环境；没有 Vulkan 1.3 loader 或合适 GPU 时明确跳过对应环境测试。验证层缺失只跳过
validation 专项，不影响其他后端测试。
