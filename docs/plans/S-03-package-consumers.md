<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-03：安装包与外部 Consumer 验证

## 状态

- 设计状态：已确认
- 实现状态：已完成；Windows/Linux 共享与静态远端矩阵已通过
- 路线图任务：S-03
- 优先级：P1
- 前置依赖：S-01、S-02

## 背景与目标

Granit 已能安装核心库和可选 RenderPipeline component，并在 Linux CI 中构建、运行共享与静态
C11/C++20 Consumer。S-03 将这些已有能力整理成明确的安装包契约，并补齐版本选择、缺失组件、
构建树污染和 Windows 动态库运行时部署等尚未覆盖的边界。

目标如下：

- 安装后的 `find_package(granit CONFIG)` 能按版本和 component 稳定选择包。
- 外部 Consumer 只依赖安装前缀，不意外引用源码树或原构建树。
- C11、C++20、共享库、静态库和 RenderPipeline component 均能独立编译、链接并运行。
- CMake 包版本、公共头文件版本和运行库版本保持一致。

## 非目标

- 当前不发布正式 ABI 稳定承诺或生成系统安装包。
- 不引入 Conan、vcpkg、CPack 或系统包管理器元数据。
- 不把测试、示例、工具和内置第三方测试库导出给使用者。
- 不承诺尚未支持的平台和编译器组合。

## 已确认决策

- `granit::granit` 是默认核心目标；`granit::render_pipeline` 通过 `RenderPipeline` component 提供。
- 包版本继续采用 `SameMajorVersion`，开发阶段仍允许破坏性 API/ABI 调整；此规则只描述 CMake
  选包行为，不代表 ABI 已稳定。
- Consumer 必须检查包、头文件和运行库的完整 major/minor/patch 三元组。
- 安装验证使用独立构建目录，并通过显式安装前缀查找，不设置源码目录 include 路径。
- 共享库 Consumer 必须从安装结果定位运行库，不能依赖开发机全局 `PATH` 或库搜索路径污染。

## 实施顺序

1. S-03A（已完成）：四类现有 Consumer 已提出版本要求，并验证包、头文件和运行库完整版本一致；
   Windows 本地共享/静态安装结果均已实际编译、链接和运行。
2. S-03B（已完成）：安装后 CMake 测试已覆盖兼容版本与精确版本成功、不同主版本和未知必需
   component 配置失败，并已接入 Linux CI 四组合。
3. S-03C（已完成）：安装清单与 CMake 导出审计已接入 Linux CI，能够拒绝测试/内置第三方
   路径、源码或构建目录绝对路径，以及 Catch2、Unity、Volk 和 Vulkan 私有依赖泄漏。
4. S-03D（已完成）：Windows Server 2022 CI 已增加 MSVC Release 共享/静态矩阵；共享 Consumer
   目录不复制 DLL，并把运行时搜索路径限制到安装 `bin` 与 Windows 系统目录。
5. S-03E（已完成）：安装指南已补齐验证矩阵、版本/component 检查、导出审计、DLL 部署和
   常见故障排查；Windows/Linux 共享与静态远端矩阵已于 2026-08-18 通过。

## 测试与验收

- Clang/GCC × 共享/静态四组合均安装并运行 C11、C++20、核心和 RenderPipeline Consumer。
- MSVC 至少覆盖共享与静态安装 Consumer，且共享运行不借用构建树 DLL。
- `find_package` 的当前兼容版本成功，不同主版本和未知必需 component 在配置阶段失败。
- Consumer 观察到的包版本、头文件宏和动态/静态库运行时版本完全一致。
- 安装导出不包含 Catch2、Unity、Vulkan include 路径或 Granit 源码绝对路径。

## 风险与未决问题

- `SameMajorVersion` 在 0.x 开发期较宽松；首次公开兼容承诺前需要与 S-01E 一并重新评估。
- 多配置生成器的安装配置和 DLL 路径不同，Windows CI 必须显式传递 `--config`。
- RenderPipeline 当前会安装若干内部静态目标作为链接闭包；其命名与公开程度在组件继续拆分时复核。
- 2026-08-16 的平台额度阻塞已解除；后续远端修复与对象库重构矩阵连续通过。详细范围见
  [CI 验证记录](../records/2026-08-18-cross-platform-ci-validation.md)。

## 完成结论

S-03 的安装包契约、版本选择测试、导出审计、四类 Consumer 和 Windows/Linux CI 配置均已实现。
Windows MSVC 共享/静态与 Linux Clang/GCC 共享/静态安装 Consumer 已完成远端验收。
