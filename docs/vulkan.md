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

物理设备选择、逻辑设备、queue、surface 和 swapchain 尚未实现。

## Loader 生命周期

Loader 状态通过函数局部静态对象初始化，依赖 C++ 的线程安全初始化保证。初始化结果在进程内缓存；
如果系统没有 Vulkan loader，返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`；loader 版本低于 Vulkan 1.3
时返回 `GRANIT_ERROR_INCOMPATIBLE_DRIVER`。

Granit 不调用全局 `volkLoadInstance` 或 `volkLoadDevice`。每个内部 instance 保存自己的
`VolkInstanceTable`，未来每个逻辑设备保存独立的 `VolkDeviceTable`，以支持多个 renderer/device
并存，避免全局函数指针被后创建的设备覆盖。

## Instance

`vulkan_instance` 是内部 move-only RAII 类型，请求 `VK_API_VERSION_1_3`。应用名称会在创建期间
复制并转换为零结尾字符串，不依赖调用者额外提供终止符。

验证层是可选能力。请求验证但缺少 `VK_LAYER_KHRONOS_validation` 或
`VK_EXT_debug_utils` 时返回 `GRANIT_ERROR_UNSUPPORTED`。当前调试回调将 warning 和 error 输出到
标准错误流；公共 renderer API 建立日志回调后，应改为通过用户提供的诊断通道发送。

## 测试策略

结果映射测试不依赖运行环境。Loader 和 instance 测试使用当前机器真实 Vulkan 环境；没有 Vulkan
1.3 loader 时明确跳过环境相关测试。验证层缺失只跳过 validation 专项，不影响基础 instance 测试。
