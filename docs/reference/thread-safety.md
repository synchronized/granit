<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 线程安全约定

本文描述当前公开 C API 的线程安全边界。C++ RAII 包装内部会修改自身句柄，因此同一个包装对象
不能并发调用 `initialize`、`reset`、`destroy`、`map` 或其他成员函数；需要并行录制时，应在同步点
之后复制其 `native_handle()` 值，并保证原包装对象在工作线程结束前存活。

## 级别定义

- **可共享只读**：同一原生句柄可以同时被多个独立 Command Recorder 或创建操作引用。
- **仅独立对象并行**：同类不同对象可以并发操作，同一对象必须由调用方排序。
- **外部独占**：对象及其父子关系在一次操作期间不能被其他线程同时修改或销毁。

内部互斥用于避免数据竞争和保护 Vulkan 外部同步要求，但不把逻辑上无序的调用变成确定工作流。
凡是依赖先后关系的写入、提交、重建和销毁，都应由调用方建立 happens-before 顺序。

## 公开对象矩阵

| 对象 | 并发创建/读取 | 同一对象并发操作 | 必须由调用方排序的操作 |
| --- | --- | --- | --- |
| Renderer | 可并发创建其独立子资源 | Queue 与 Pipeline Cache 在内部串行化 | Renderer 销毁与全部子对象操作 |
| Surface | 创建后可被 Swapchain 创建读取 | 不提供可变操作 | Surface/Renderer 销毁与 Swapchain 操作 |
| Swapchain | 不同 Swapchain 可并行 | 不允许 | acquire、present、cancel、recreate、查询与销毁 |
| Frame 令牌 | 不适用 | 不允许 | submit、present、cancel，只能消费一次 |
| Buffer | 不同 Buffer 可并行创建、上传和销毁 | 多个 Recorder 可共享只读 Buffer | map、unmap、write、销毁及任何写依赖 |
| Texture | 不同 Texture 可并行创建、上传和录制 | 多个 Recorder 可共享只读 Texture | 同一 Texture 的 write、附件写入、销毁及写后读顺序 |
| Texture View | 创建后可共享只读 | 可被多个 Recorder 引用 | View/父 Texture 销毁与新的引用操作 |
| Sampler | 创建后可共享只读 | 可被多个 Bind Group 引用 | 销毁与新的引用操作 |
| Shader | 创建后可共享只读 | 可被多个 Pipeline 创建操作引用 | 销毁与新的 Pipeline 创建 |
| Bind Group Layout | 创建后可共享只读 | 可被多个 Layout/Bind Group 创建引用 | 销毁与新的引用操作 |
| Pipeline Layout | 创建后可共享只读 | 可被多个 Pipeline 和 Recorder 引用 | 销毁与新的引用操作 |
| Bind Group | 不可变，可共享只读 | 可被多个独立 Recorder 绑定 | 销毁与新的绑定操作 |
| Graphics/Compute Pipeline | 不可变，可共享只读 | 可被多个独立 Recorder 绑定 | 销毁与新的绑定操作 |
| Command Recorder | 不同 Recorder 可并行录制 | 不允许；无并发时可以跨线程移交 | begin/end/submit/reset/destroy 的状态顺序 |

已经成功录制的 Command Recorder 会持有所引用资源的内部所有权。之后销毁公开资源句柄不会使该
Recorder 中已经录制的命令悬空；但销毁与“正在通过公开句柄建立新引用”的调用仍必须由用户排序。

## Queue 与资源依赖

同一 Renderer 的 Queue 提交由内部锁串行化。多个线程可以提交不同 Recorder，但锁竞争决定实际
顺序，因此不能依赖线程启动或录制完成顺序。存在资源依赖时，应在调用层显式确定提交顺序。

不同 Recorder 可以并行录制对同一不可变资源的读取。对同一 Buffer 或 Texture 的写入也可以先在
不同 Recorder 中独立录制，但最终内容和状态由提交顺序决定；写后读、读后写以及多写依赖必须由
调用方排序。第一版不提供任务图、Future 或跨 Queue 自动调度。

## 回调与平台对象

当前基础渲染 API 没有用户回调。Win32 窗口句柄的线程亲和、消息循环和销毁顺序仍由调用方遵守
平台规则；Granit 的内部锁不会放宽操作系统对窗口对象的要求。
