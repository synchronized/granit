<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# P-01：并行命令记录、资源创建与上传压力测试

## 元数据

- 设计状态：已确认
- 实现状态：进行中（P-01A、P-01B 已完成）
- 路线图任务：P-01
- 优先级：P1
- 前置依赖：阶段五

## 线程边界

- 不同 Command Recorder 可以由不同线程同时创建和录制。
- 单个 Command Recorder 同一时刻只能由一个线程操作；内部互斥保证误用不会产生数据竞争，但不把
  同一 Recorder 的并发调用定义为有效工作流。
- Buffer、Texture 等不同资源允许并发创建和上传；同一资源的写入、映射、销毁与录制引用不能由
  用户无序并发执行。
- Queue 提交仍由 Renderer 内部串行化。并行录制完成后可由调用者选择线程提交，但提交顺序决定
  跨 Recorder 的资源状态顺序。
- 第一版不内置线程池、任务图或 Future，线程调度由上层负责。

## 分步实施

1. P-01A / 已完成：独立 Buffer 上传、独立 Recorder 创建与并行录制压力测试，主线程顺序提交。
2. P-01B1 / 已完成：Texture/View 创建与独立颜色附件命令压力测试。
3. P-01B2 / 已完成：以源数据布局和目标区域描述 Texture 写入，覆盖独立 Texture 并发上传压力
   测试；当前 staging buffer 为过渡实现，P-04 再建立可复用上传分配器。
4. P-01C：并行录制 Graphics/Compute 工作负载，并覆盖共享只读资源与独立写资源。
5. P-01D：整理公开对象线程安全矩阵，作为 P-02 测量的基线。

## 验收标准

- Validation Layer 下无外部同步、生命周期或资源状态错误。
- 压力测试不依赖固定线程调度顺序，失败能够通过结果码汇总报告。
- Registry 全局锁不覆盖命令录制本身；Queue 和共享资源状态仍保持确定顺序。
- Clang 共享库、Visual Studio 共享库和 Clang 静态库三套矩阵通过。
