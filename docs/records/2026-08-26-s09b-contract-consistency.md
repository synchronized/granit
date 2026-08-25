<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-26 S-09B 公共契约一致性审计

## 结论

S-09B 已完成。Core、RenderPipeline、Window 和 Input 的公共创建及代表性操作路径已经统一区分
参数错误与无效句柄；C++ 包装与 C ABI 的返回语义保持一致，失败输出和移动后状态已有回归覆盖。

本轮没有新增或移除公共 ABI，也没有改变结构体布局。主要改动是修正现有实现与
[核心 C API 契约](../reference/c-api-contract.md)之间的不一致。

## 已修复范围

- 空 Renderer、Window System、Surface 及描述结构中的资源句柄返回 `INVALID_HANDLE`。
- 无效 `struct_size`、空输出指针、空批次和越界数量继续返回 `INVALID_ARGUMENT`。
- 创建失败时输出资源句柄保持为零。
- C++ RAII 包装在底层资源已失效时清空本地状态，同时向调用方返回 `INVALID_HANDLE`。
- 重复 C 销毁返回 `INVALID_HANDLE`；空 C++ 对象的 `reset()` 保持幂等成功且不再次跨 ABI。
- 跨父资源混用、旧 generation、移动后源对象和父 Renderer 级联失效继续由既有测试覆盖。

## 本地提交

- `469acf1`：统一 RenderPipeline 空父句柄语义。
- `f4adb3d`：统一 Core 资源空父句柄语义。
- `1cc7d8c`：统一剩余创建接口句柄语义。
- `ac4bed2`：补齐非创建路径句柄语义。
- `f5ba067`：清理失效 RAII 句柄状态。

## 验证结果

Windows Clang Debug 完整构建成功，随后运行 47 项 CTest，结果为 47/47 通过。覆盖内容包括：

- C11/C++20 头文件、版本和 ABI 导出检查；
- Renderer、资源、Pipeline、RenderPipeline、Window 和 Input；
- 句柄表、生命周期、跨 Renderer 与 Vulkan 后端；
- 安装相关示例、SDL3、ImGui 和文档链接检查。

S-09 的下一阶段是 S-09C 安装与文档闭环；跨平台共享/静态 Release 与独立安装 Consumer 仍由
后续阶段验证。
