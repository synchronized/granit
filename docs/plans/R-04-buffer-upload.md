<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-04：Buffer 初始数据与同步上传

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-04
- 优先级：P0
- 前置依赖：R-01、R-02、R-03
- 后续依赖：F-01、R-08

## 已确认语义

- 保留 `granit_buffer_create`，新增 `granit_buffer_create_with_data` 和
  `granit_buffer_write`，不破坏现有 ABI。
- 初始数据只在调用期间借用，且必须覆盖完整 Buffer。
- `UPLOAD` 直接复制到内部持久映射并 flush。
- `DEVICE` 和 `AUTOMATIC` 通过临时 staging Buffer、graphics queue 和 Fence 同步上传。
- `READBACK` 拒绝初始数据和写入。
- write 支持非空局部范围；越界、映射期间写入和空数据均被拒绝。
- 成功返回表示复制已完成，调用者可以立即释放源数据。

## 内部实现

首版每次 device-local 写入创建 transient Command Pool、一次性 Command Buffer 和 Fence。上传
流程在 Renderer 的 Queue 互斥锁下记录、提交并等待本次 Fence，不调用 `vkQueueWaitIdle`。

目标 DEVICE/AUTOMATIC Buffer 内部自动带 transfer destination 用途，但该实现细节不改变公共
usage 的含义。临时 staging 和目标 Buffer 在任一步失败时成对清理；带数据创建失败时输出句柄
保持零值。

该路径面向初始化和低频更新，不作为高吞吐逐帧上传方案。后续批量 Upload Context 或 Ring
Allocator 可以复用相同 C ABI，或提供独立高级接口。

## 测试与结果

- C API 参数测试覆盖空数据和无效句柄。
- 真实设备测试覆盖 DEVICE 完整初始数据、DEVICE 局部写入、UPLOAD 初始数据和越界拒绝。
- READBACK 带数据创建返回不支持且不产生句柄。
- Clang 动态库和 MSVC 静态库均需通过完整测试矩阵。
