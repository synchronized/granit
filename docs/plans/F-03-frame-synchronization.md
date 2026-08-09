<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-03：帧同步内部抽象

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：F-03
- 优先级：P0
- 前置依赖：F-01、F-02、R-08A
- 后续依赖：F-04、F-06、R-08B

## 目标

在不扩大公共 ABI 的前提下，建立 Queue 提交和交换链帧循环所需的 Vulkan 同步对象所有权：

- Fence 封装 GPU 提交完成点；
- 两个二进制 Semaphore 分别连接 acquire 与提交、提交与 present；
- 每帧上下文将这三个对象组织成一个不可复制的槽位；
- 对象只保存于 Vulkan 后端，不向普通用户暴露 `VkFence` 或 `VkSemaphore`。

## 第一版模型

每个 frames-in-flight 槽位包含：

1. `completion_fence`：创建时为已触发状态，保证第一帧不会等待一个从未提交的 Fence；
2. `image_available`：由 swapchain acquire 触发，Queue 在颜色附件阶段前等待；
3. `render_finished`：由 Queue 提交触发，present 在使用图像前等待。

Fence 在 F-04 中按“等待上一轮 → 回收完成资源 → 即将提交前复位 → 提交并绑定 Fence”的顺序
使用。复位不能提前到 acquire 之前，否则 acquire 失败时可能留下永远不会触发的 Fence。

## 生命周期与错误

- 创建任一同步对象失败时，按相反顺序销毁已经创建的对象。
- 销毁 Renderer 前先停止新提交并等待 Device 空闲，再销毁所有帧上下文。
- Fence 和 Semaphore 不可复制、不可移动，F-04 使用稳定地址或间接所有权保存可配置数量的槽位。
- Vulkan 错误统一映射为 `granit_result`；等待超时的公共语义在真正公开非阻塞等待前再确定。

## 本任务不包含

- 不公开 Fence、Semaphore 或提交序号句柄。
- 不实现 Queue 提交、frames-in-flight 轮转或 Queue 锁；这些属于 F-04。
- 不执行 swapchain acquire/present；这些属于 F-06。
- 不推进资源完成序号或收集退役资源；这些随 F-04 接入 R-08B。
- 不加入资源 Barrier 和 Layout 转换；这些属于 F-05。

## 验收结果

- 后端可以创建并销毁一个完整帧上下文。
- 初始 Fence 可用零超时成功等待。
- 重复初始化会被拒绝，部分初始化失败会自动清理。
- 设备初始化校验所有必需的 Fence/Semaphore 函数指针。
- Vulkan 后端测试覆盖真实同步对象的初始状态和生命周期。

## 实现差异

路线图同时提到 Fence 与 Semaphore。第一版采用传统的每帧 Fence，而没有立即引入 Timeline
Semaphore。当前每帧只对应一次 Queue 提交，该方案的完成点与槽位复用关系更直接；如果未来需要
一个槽位包含多个提交或跨 Queue 同步，再评估用 Timeline Semaphore 替代内部完成 Fence。该变化
不会影响公共 ABI。
