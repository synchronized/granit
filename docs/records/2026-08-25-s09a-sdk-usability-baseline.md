<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-25 S-09A 公共 SDK 使用路径基线

## 结论

从 Release 安装前缀独立配置、构建并运行的七个 Consumer 已冻结四条公共 SDK 使用路径。Core C、
Core C++ 和 RenderPipeline 不再只做头文件或版本检查，已经覆盖真实 Renderer、Buffer、Pipeline
创建与清理；Window/Input 继续覆盖平台成功或明确不支持分支。

本轮没有发现必须新增 C ABI 才能完成的路径。发现一项结果码不一致和三项文档/覆盖缺口，统一留给
S-09B～S-09D 审计，不在基线阶段改变行为。

## 环境与方法

- 日期：2026-08-25
- 系统：Windows 10 AMD64
- 编译器：Clang 22.1.8
- 生产者构建：`windows-clang-release` 与 `windows-clang-static-release`
- 安装入口：`cmake --install` 写入独立前缀
- Consumer：独立 CMake project，只通过 `find_package(granit 0.3 CONFIG REQUIRED ...)` 使用 SDK
- 运行时：只把安装前缀的 `bin` 加入 `PATH`，Consumer 目录不复制 Granit DLL

调用次数只统计 Consumer 可见的公共动态库调用，不展开单个 API 内部的 Vulkan 或模块调用。C++
包装的空对象 `reset()` 不跨 ABI，因此与 C 的重复销毁调用分开记录。

## 四条代表性路径

| 路径 | Consumer | 成功路径 | 失败/边界 | 最少公共调用 |
|---|---|---|---|---:|
| Core C | `granit_c_consumer` | 版本、Renderer、UPLOAD Buffer、逆序清理 | 短结构、Buffer/Renderer 重复销毁 | 10 |
| Core C++ | `granit_cpp_consumer` | Renderer、Buffer、move-only RAII | 移动后空状态、重复 `reset()` | 7 |
| RenderPipeline C/C++ | 两个 Pipeline Consumer | Renderer、默认 Pipeline 创建与清理 | 空 Renderer、旧句柄、移动后状态 | 7 / 6 |
| Window/Input | Window C、Input C/C++ | Window System、Input System、逆序清理 | 后端不支持、重复销毁/`reset()` | 3 / 6 / 4 |

Core C 的短 `struct_size` 必须返回 `INVALID_ARGUMENT` 且保持输出句柄为零；重复销毁旧 Buffer 和
Renderer 句柄必须返回 `INVALID_HANDLE`。C++ 包装移动后源对象为空，首次与重复 `reset()` 都成功，
第二次不调用动态库。

RenderPipeline 的默认创建会建立内部参考资源，因而验证了 component 与 Core 的真实链接、资源
所有权和销毁顺序。当前 Consumer 尚未提交实际 Scene 渲染；这属于安装示例闭环缺口，而不是创建
第二套测试专用 API 的理由。

## 平台差异

- Renderer 无可用 Vulkan 后端、驱动或设备时，Core 与 RenderPipeline Consumer 接受
  `BACKEND_UNAVAILABLE`、`INCOMPATIBLE_DRIVER` 或 `NO_SUITABLE_DEVICE`，但输出句柄必须保持零。
- Windows Window/Input 路径必须成功创建并清理系统对象。
- 当前 Linux Window component 没有通用默认后端选择时，可以返回 `UNSUPPORTED` 或
  `BACKEND_UNAVAILABLE`；Input Consumer 随之安全跳过，且 Window 句柄必须为零。
- 共享与静态包必须提供相同 component 组合和运行结果；最终跨平台矩阵由 PR CI 验收。

## 待后续阶段处理

1. **S-09B 结果码（已修复）**：基线时 `granit_render_pipeline_create` 的空 Renderer 返回
   `INVALID_ARGUMENT`；后续已按 Core 与 RenderPipeline 契约统一为 `INVALID_HANDLE`，Scene
   Snapshot 的相同验证入口也同步修复。
2. **S-09B/C 清理语义文档**：C 重复销毁返回 `INVALID_HANDLE`，C++ 空对象 `reset()` 幂等成功；
   行为合理但用户文档需要明确，不能只靠测试推断。
3. **S-09C RenderPipeline 路径**：安装 Consumer 已覆盖真实创建，尚缺只依赖已安装 SDK 的最小实际
   render 路径或等价教程命令。
4. **S-09D 诊断**：失败分支已有可区分结果码，但 Consumer 尚未验证诊断回调能指出参数、句柄类型
   或生命周期位置。

## 验证结果

共享与静态安装包中的以下七个程序均返回零：

- `granit_c_consumer`
- `granit_cpp_consumer`
- `granit_pipeline_c_consumer`
- `granit_pipeline_cpp_consumer`
- `granit_window_c_consumer`
- `granit_input_c_consumer`
- `granit_input_cpp_consumer`

本记录是实施证据，不取代 Core、RenderPipeline、Window 或 Input Reference 中的正式契约。
