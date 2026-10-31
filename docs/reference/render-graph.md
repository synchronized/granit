<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 公共 Render Graph（实验性）

公共 Render Graph 是独立的 `RenderGraph` component。外部引擎可以只链接
`granit::render_graph` 和核心 `granit::granit`，保留自己的场景、材质、Pipeline 和 Pass 实现。

## 构建与边界

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS RenderGraph)
target_link_libraries(game PRIVATE granit::render_graph)
```

公共入口是 `granit/render_graph.h` 和 `granit/render_graph.hpp`，不需要包含 `src/` 或 Vulkan
头文件。Graph 不拥有 Renderer，也不负责加载文件或创建 Pipeline。

使用 C++ 入口时，描述数据使用 `granit::render_graph_desc`、
`granit::render_graph_pass_desc`、`granit::render_graph_access_desc`、
`granit::texture_desc` 和 `granit::buffer_desc`。这些类型只在包装层转换为 C ABI 描述结构；
`native_handle()` 仅用于明确的底层互操作，不表示 C++ 包装层接管句柄所有权。

## 资源与 Pass

调用方可以导入已有 Buffer、Texture View，或创建图内临时 Buffer、Texture。导入资源是借用的，
调用方必须保证资源在 Graph 执行及 Renderer GPU 使用期间有效；Graph 不会销毁导入资源。
临时资源由 Graph 管理，在提交后交给 Renderer 的延迟销毁机制，录制或提交失败也会清理已创建资源。

每个 Pass 需要提供名称、资源访问声明和录制回调。访问方向为 `READ`、`WRITE` 或
`READ_WRITE`，用途为 `SAMPLED`、`STORAGE`、附件或传输用途。用途描述意图，不暴露 Vulkan
Image Layout；状态转换由 Graph 与 Renderer 内部协作完成。

回调只能在调用期间使用上下文取得的 Recorder、Buffer 和 Texture View，不能保存这些句柄或上下文。
Pipeline 和用户数据由调用方管理。回调返回失败时，执行结果会提供阶段、Pass、资源和依赖错误信息。

## 执行模型

Graph 可以重复执行。离屏执行时传入空 Frame；输出到窗口时传入 Renderer 获取的 Frame。调整尺寸
应更新临时 Texture 描述并重新编译。首期不支持并行录制、异步队列、自动 Subpass 合并或
Vulkan input attachment；需要局部读取时拆成独立 Pass，并把前一 Pass 的输出作为 sampled 输入。

完整的生命周期、错误和 toy-engine 迁移契约见
[S-113 公共 Render Graph API 计划](../plans/S-113-public-render-graph-api.md)。
