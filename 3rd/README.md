<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 第三方依赖

本目录保存 Granit 可复现构建所需的第三方源码。除 Granit 自己的 CMake 包装外，不直接修改
上游源码。

| 依赖 | 版本 | 来源 | 许可证 | 用途 |
| --- | --- | --- | --- | --- |
| Catch2 | 3.15.0 | <https://github.com/catchorg/Catch2/tree/v3.15.0> | BSL-1.0 | 测试框架 |
| Unity | 2.6.1 | <https://github.com/ThrowTheSwitch/Unity/tree/v2.6.1> | MIT | C API 测试框架 |
| Vulkan-Headers | 1.4.350 | <https://github.com/KhronosGroup/Vulkan-Headers/tree/v1.4.350> | Apache-2.0 | 内部 Vulkan 声明 |
| Volk | 1.4.350 | <https://github.com/zeux/volk/releases/tag/1.4.350> | MIT | Vulkan 函数加载 |
| Vulkan Memory Allocator | 3.3.0 | <https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases/tag/v3.3.0> | MIT | 内部 GPU 内存分配 |

Vulkan-Headers 和 Volk 必须使用匹配的 Vulkan registry 版本成对升级。Granit 当前编译使用
1.4.350 头文件，但运行时最低目标仍为 Vulkan 1.3。

Unity 只测试公开 C API；Catch2 用于 C++20 包装层和内部实现。两者仅在启用测试时构建，
不会成为 Granit 库或安装包的依赖。

Vulkan Memory Allocator 仅编译进 Granit 内部，由 Volk 提供函数指针，不进入公共头文件或安装
导出。
