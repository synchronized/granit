<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-01：Command Recorder 基础

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：F-01
- 优先级：P0
- 前置依赖：R-08A、R-09
- 后续依赖：F-02、F-03、F-04

## 目标

- 通过 64 位不透明句柄公开一次录制、一次提交的 Command Recorder。
- 每个 Recorder 独占 Vulkan Command Pool 和一个主 Command Buffer。
- 明确定义 initial、recording、executable、pending 和 invalid 状态。
- 不同 Recorder 可以由不同线程并行录制。
- 为 F-02 命令、F-04 提交和 R-08B 资源引用跟踪提供稳定载体。
- Vulkan 类型、Queue Family 和 Command Pool 生命周期完全留在动态库内部。

## 非目标

- F-01 不提供 draw、copy、barrier 或 begin rendering 命令。
- 不提交 Queue，不创建 Fence 或 Semaphore。
- 不支持 Secondary Command Buffer、Bundle 或可重复并发提交。
- 不实现线程池、任务调度器或强制线程绑定。
- 不公开原生 `VkCommandBuffer`。

## 公共 API

C API 使用 `granit_command_recorder` 整数句柄，并提供 create、begin、end、reset 和 destroy。
描述结构第一版只有 `struct_size`、flags 和保留字段，ABI 大小为 16 字节。flags 当前必须为零。

C++20 层提供无异常、move-only 的 `granit::command_recorder`，只保存 Renderer 和 Recorder 两个
句柄，不维护平行状态机。

## 状态机

```text
create → initial
initial → begin → recording
recording → end → executable
executable → reset → initial
initial → reset → initial
任意非 pending 状态 → destroy → invalid
```

F-04 已增加 `executable → submit → pending → GPU 完成 → reset → initial`。

- 只有 initial 可以 begin。
- 只有 recording 可以 end。
- recording 和 pending 不能 reset。
- 状态错误返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- 句柄类型、generation 或 Renderer domain 错误返回 `GRANIT_ERROR_INVALID_HANDLE`。
- `vkEndCommandBuffer` 失败会进入 invalid，成功 reset 后可以恢复。

## 后端所有权与线程

每个 Recorder 第一版独占一个可 reset 的 graphics Command Pool、一个 Primary Command Buffer
和一个内部互斥锁。独占 Pool 避免不同 Recorder 共享 Vulkan 外部同步对象；F-04 可以在内部
优化 Pool 分配，但不能改变公共语义。

单个 Recorder 同一时刻只允许一个线程调用，但不永久绑定创建线程，用户可在无并发时移交。
不同 Recorder 可以并行操作。Registry 锁只完成句柄查找，Vulkan 操作在 Registry 锁外执行。

## 资源生命周期

F-02 记录资源命令时，Recorder 校验公开句柄并保存内部强引用。用户随后销毁公开句柄不能使已
录制或提交的命令悬空。F-04 保留引用至成功 reset 或 destroy；逐资源 `last_use_serial` 和
`retirement_queue` 转移仍由 R-08B 完成。

Renderer 销毁会首先使 Recorder 句柄失效，等待全部在途提交，再销毁 Command Pool 并级联销毁
其他资源。验证模式下，遗漏 Recorder 会进入 V-01 汇总。

## 动态库调用策略

第一版每条命令通过 C API 进入 Recorder，保持 ABI 简单、验证集中。F-02 对高频重复参数提供
批量入口，避免把每个细粒度元素变成一次 DLL 调用。当前不公开自定义命令字节码，以免形成
第二套需要长期兼容的命令流 ABI。

## 验证与结果

- C11/C++20 头文件可独立包含，C ABI 描述固定为 16 字节。
- 空 Command Buffer 可以 begin/end/reset 后再次录制。
- 重复 begin/end、录制中 reset 和未知 flags 被拒绝。
- 跨 Renderer domain、重复销毁和 Renderer 级联销毁正确失效。
- C++ 包装保持 move-only，移动后原对象无效。

已完成 Recorder 句柄、独占 Pool/Primary Command Buffer、状态机、线程互斥、C/C++ 包装、
Renderer 级联销毁和生命周期统计。提交与 pending 状态转换按计划留给 F-04。

Windows Clang + Ninja Debug 动态库和 Visual Studio 2022 Debug 静态库均在严格警告下构建，
全部测试通过。
