<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 变更记录

本文件记录面向使用者的公共接口、行为、构建和兼容性变化。项目当前仍处于 0.x；`Unreleased`
内容不代表已经发布。版本兼容规则见[版本与兼容策略](docs/reference/compatibility.md)。

## Unreleased

### 新增

- Renderer 预留 Dynamic Uniform Buffer Binding 类型，并为 Graphics/Compute Bind Group 绑定
  增加统一的版本化描述和动态 Offset 数组。

### 兼容性与迁移

- `granit_command_recorder_bind_graphics_groups` 和
  `granit_command_recorder_bind_compute_groups` 改为接收 `granit_bind_groups_desc`；旧参数需要迁移
  到初始化后的描述结构。详见[从 0.3 迁移到 0.4](docs/guides/migrate-0.3-to-0.4.md)。
- 项目开发版本已提升到 0.4.0；0.3.0 使用者必须重新编译，不承诺二进制兼容。

## 0.3.0 - 2026-08-26

### 新增

- Core 新增 Frame Context、帧槽查询和显式 Buffer flush；RenderPipeline 新增 Canvas 批量追加，
  用于复用真实在途帧槽并减少逐项跨 ABI 调用。
- Renderer validation 诊断可定位代表性的 Buffer 描述错误、失效句柄和跨 Renderer 句柄；结果码
  仍是程序逻辑的稳定依据。

### 修复

- RenderPipeline component 的创建接口把空 Renderer 统一归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并在失败时保持输出句柄为零。
- Buffer、Command Recorder、Frame Context、Sampler、Texture 和 Timestamp Query Pool 的创建接口
  同步采用相同的空 Renderer 语义，C++ 包装与 C 接口保持一致。
- Surface、Swapchain、底层 Pipeline、Window 和 Input 创建接口统一把空父资源及资源字段归类为
  `GRANIT_ERROR_INVALID_HANDLE`，并保持失败输出为零。
- Texture View、Shader、Upload Batch、Recorder 批量提交和 Pipeline Cache 操作补齐相同的
  无效句柄语义，保留空批次等参数形状错误为 `GRANIT_ERROR_INVALID_ARGUMENT`。
- C++ RAII 包装在底层句柄或父资源已失效时，`reset()` 返回 `INVALID_HANDLE` 的同时清空本地
  状态，避免对象继续表现为有效或在析构时重复销毁。

### 工程化

- 独立安装 Consumer 注册为 CTest，并自动补充安装共享库的运行时搜索路径；构建指南可用一条
  `ctest` 命令验证 Core、RenderPipeline、Window 和 Input 的七条 C/C++ 路径。
- RenderPipeline C++ 安装 Consumer 通过公开阶段回调执行真实离屏渲染图，覆盖 Scene Snapshot、
  输出纹理、阶段录制、提交和清理，不依赖源码树资源。
- 文档检查锁定 README、安装 Consumer 和 RenderPipeline 教程的关键 CMake/CTest 命令，避免
  文档入口随构建配置漂移。
- Windows/Linux Actions 的安装 Consumer 统一使用与构建指南相同的 CTest 入口，覆盖共享与静态
  安装矩阵并由测试自身设置运行库路径。

### 兼容性与迁移

- 未删除或改名 0.2.0 的公共 C 导出；新增导出属于兼容扩展。
- `granit_canvas_draw_list_desc` 将一个原保留字段定义为 `frame_slot_count`，
  `granit_canvas_record_desc` 新增 `frame_slot` 并扩大 V1 尺寸。旧代码必须使用当前初始化宏重新编译，
  不应手写结构大小或复用 0.2.0 二进制描述布局。
- 空父资源、失效句柄和跨对象归属错误现在统一返回 `GRANIT_ERROR_INVALID_HANDLE`；只比较
  `GRANIT_ERROR_INVALID_ARGUMENT` 的旧错误分支需要同步接受新分类。
- 完整迁移步骤见[从 0.2 迁移到 0.3](docs/guides/migrate-0.2-to-0.3.md)。0.3.0 仍不承诺稳定
  C ABI 或 C++ 二进制 ABI。

## 0.2.0 - 2026-08-24

### 新增

- 核心 Renderer C ABI、C++20 RAII 包装及 Windows/Linux 共享、静态安装 Consumer 验证。
- RenderPipeline、Window、Input、SDL3 Integration 和 ImGui Integration component。
- Granit 0.1.0 基线上的 Core、RenderPipeline、Window 和 Input C ABI 回归快照，以及
  component 级所有权、错误、线程和扩展契约。

### 修复

- RenderPipeline 的公开描述结构提供固定 V1 尺寸，未知尾部可以按统一规则忽略。
- Input 事件与状态输出按调用方 `struct_size` 容量写入，避免旧结构缓冲区越界。
- 核心 Pipeline 销毁接口统一为空句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。

### 兼容性

- 0.2.0 仍是非稳定版本；0.x 次版本可包含有迁移说明的破坏性变更。
