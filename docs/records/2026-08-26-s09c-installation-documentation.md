<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-26 S-09C 安装与文档闭环

## 结论

S-09C 已完成。Core、RenderPipeline、Window 和 Input 的七个独立 C/C++ Consumer 现在可通过
统一 CTest 入口执行，并自动使用安装前缀中的共享库。README、构建指南和 RenderPipeline 教程的
关键命令已进入文档检查。

RenderPipeline C++ Consumer 不再停留在创建检查，而是仅依赖已安装 SDK 创建 Scene Snapshot、
输出纹理和阶段回调，并执行实际离屏渲染图；该路径不访问源码目录、内部模块或示例资源。

## 本地安装矩阵

| 平台与配置 | 链接模式 | Consumer 结果 |
|---|---|---:|
| Windows Clang Release | 共享 | 7/7 |
| Windows Clang Release | 静态 | 7/7 |

两组验证均从独立安装前缀重新配置 Consumer。共享验证没有向 Consumer 目录复制 DLL；静态验证由
CMake 导出目标闭包传递内部依赖。

## 远端验证

Draft PR [#20](https://github.com/synchronized/granit/pull/20) 的以下任务全部通过：

- Linux Clang：共享、静态；
- Linux GCC：共享、静态；
- Linux Integration Runtime：共享、静态；
- Windows MSVC 安装 Consumer：共享、静态。

对应 [Linux Actions](https://github.com/synchronized/granit/actions/runs/32916524500) 和
[Windows Actions](https://github.com/synchronized/granit/actions/runs/32916524514) 均成功完成。

## 后续

S-09 的下一阶段是 S-09D 诊断可执行性。S-09C 不新增公共 ABI，也不把尚未安装的内部 Material、
Lighting 或 Render Graph 模块描述为外部 SDK 入口。
