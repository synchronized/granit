<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Window 与 Input component 契约

本文汇总 Window 和 Input 的依赖、所有权、错误、结构扩展及线程规则。各平台事件映射仍以
[Window](window.md)和[Input](input.md)专题参考为准。

## Component 与依赖

- `Window` 提供 `granit::window`，不依赖 Renderer 或 Vulkan。
- `Input` 提供 `granit::input`，公开依赖 `granit::window`，平台适配和 xkbcommon 保持为私有实现。
- 两者使用独立动态库和导出宏；Core 或 RenderPipeline 稳定不自动冻结 Window/Input ABI。

## 所有权与销毁顺序

- Window System 拥有其创建的 Window；单独销毁 Window 会使句柄立即失效，销毁 Window System
  会级联销毁剩余 Window。
- 窗口状态查询只复制尺寸与缩放值；原生窗口、Display、Connection 和 Surface 查询只借出值，
  不转移所有权。
- 一个 Input System 借用并独占附着一个 Window System。Input System 必须先于 Window System
  销毁；单个 Window 可以在 Input System 前后销毁。仍附着 Input 时销毁 Window System 返回
  `GRANIT_ERROR_INVALID_ARGUMENT`。
- Window 销毁会同步移除对应输入状态和排队事件，Input 不延长 Window 生命周期。

## 错误与输出

- 空指针、非法描述、跨线程调用和错误销毁顺序返回
  `GRANIT_ERROR_INVALID_ARGUMENT`；失效、类型错误或归属错误的句柄返回
  `GRANIT_ERROR_INVALID_HANDLE`。
- 请求与当前平台或已选后端不匹配的原生值返回 `GRANIT_ERROR_UNSUPPORTED`，输出值清零。
- 平台连接不可用返回 `GRANIT_ERROR_BACKEND_UNAVAILABLE`；事件队列为空返回
  `GRANIT_ERROR_NOT_READY`。
- 创建失败时输出句柄为零。事件、窗口状态和输入状态查询在失败时返回已清零的已知字段，调用者
  只应在成功时使用业务数据。

## `struct_size` 与输出容量

- 所有可扩展结构均提供固定的 `*_VERSION_1_SIZE`；输入描述小于最低尺寸时返回参数错误，未知
  尾部被忽略。
- 调用者使用 `*_INIT` 初始化事件和状态输出。函数先读取调用者提供的 `struct_size` 作为容量，
  只写入容量与当前结构大小的较小值，不覆盖调用者的未知尾部。
- 返回结构中的 `struct_size` 表示本次实际写入的字节数。Keyboard 和 Pointer 的 V1 尺寸不包含
  保留尾部，因此旧调用方只分配 V1 大小时也不会发生越界写入。

## 线程规则

Window System 和 Input System 都绑定 Window System 的创建线程。窗口创建/销毁、事件泵、原生值
查询、Input 创建/销毁、事件轮询及状态查询必须在该线程执行；跨线程调用返回参数错误。平台回调
不会把异常传播过动态库或原生事件边界。
