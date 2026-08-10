<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-02：Shader Module 生命周期

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：D-02
- 优先级：P0
- 前置依赖：D-01
- 后续依赖：D-03、D-04、D-06

## 目标

在不暴露 Vulkan 的前提下提供 SPIR-V Shader 对象，覆盖输入校验、动态库 ABI、Renderer domain、
Device Lost、级联销毁和 C++ RAII。

## 公共接口

- `.h` 定义 `granit_shader`、Vertex/Fragment/Compute 阶段和版本化创建描述。
- 描述使用 `const void* + uint64_t` 表示 SPIR-V 字节，不要求调用者满足 `uint32_t` 对齐。
- 入口点使用指针与显式长度，第一版上限为 255 字节且不允许嵌入零字符。
- C++ `shader` 是 move-only RAII 对象，通过 `std::span<const std::byte>` 接收代码。
- 创建成功后 Granit 不再引用输入内存；调用者可以立即释放或复用原缓冲。

## 输入校验

公共入口在调用 Vulkan 前检查：

- 描述版本、保留字段、阶段和入口点；
- SPIR-V 至少包含五个 Header word；
- 总长度为 4 的倍数且不超过 64 MiB；
- 第一个 word 为 SPIR-V Magic Number。

C API 将任意对齐的输入复制到内部 `uint32_t` 数组，再调用 `vkCreateShaderModule`。完整指令合法性
仍由 D-01 定义的离线工具负责，运行时不重复实现 SPIR-V 验证器。

## 内部生命周期

- Registry 记录 Shader 的 Renderer、Vulkan Shader Module、阶段、入口点和创建序号。
- 句柄表校验资源类型、generation 和 Renderer domain。
- Shader 销毁立即移除公开句柄，并进入普通 GPU 资源退役队列。
- Renderer 销毁时 Shader 参与生命周期遗漏诊断并被级联释放。
- Device Lost 后拒绝新建 Shader，但销毁路径仍可清理 CPU 和 Vulkan 对象。
- Vulkan Device 初始化验证 Shader Module 创建/销毁函数入口存在。

## 测试结果

- C11/C++20 公共头独立编译通过。
- 真实 Vulkan Shader Module 创建和销毁通过。
- 创建后覆盖原 SPIR-V 输入不会影响 Shader 生命周期。
- 覆盖错误 Magic Number、非四字节长度、空入口点、错误阶段和跨 Renderer 销毁。
- Windows Clang 与 Visual Studio 2022 的完整测试矩阵通过。

## 与 D-03 的边界

D-02 保存阶段和入口点，但不创建 Pipeline Layout、Descriptor Set Layout 或 Graphics Pipeline。
D-03 使用 Shader 内部记录构建 Pipeline，并在真实绑定需求明确后确定最小反射元数据格式。
