<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-113：面向外部引擎的公共 Render Graph API

## 状态

提案，尚未实现。目标版本暂定为 `v0.71.0`，具体版本范围以版本文档为准。

## 背景与目标

当前仓库已有 `src/render_graph` 串行图原型，供参考 RenderPipeline 内部使用。它不能作为外部
引擎接口：头文件位于 `src/`，依赖 C++ 容器和内部目标，没有公共 C ABI，也没有完整的资源状态
和 GPU 生命周期契约。

本计划为 toy-engine 等外部引擎提供独立的 Render Graph component，使调用方保留自己的场景、
材质、可见性和 Pass 实现，只使用 Granit Renderer 负责 GPU 资源、命令、同步和提交。

首期覆盖：

- 外部 Buffer、Texture View 的借用式导入；
- 图内临时 Buffer、Texture 的创建和使用范围管理；
- 命名 Pass、资源读写声明、显式先后依赖和拓扑排序；
- 录制回调中的 Renderer、Command Recorder 和资源查询；
- 离屏执行以及与已获取 Frame 配合的窗口输出；
- 编译、录制、资源创建、提交和生命周期失败诊断；
- 独立安装 SDK Consumer，证明不依赖 `src/` 私有头文件或 RenderPipeline。

## 非目标

- 并行录制、多队列异步计算、显存别名复用和自动 Subpass 合并；
- PBR、Scene、Material、Shadow、Particle、UI 或 Tone Mapping 的业务实现；
- 在回调中隐式加载资产、冷编译 Pipeline 或调用 Vulkan；
- 自动把现有 Vulkan Subpass 绘制实现原样迁移到公共 Graph；
- 暴露 Vulkan Render Pass、Image Layout、Barrier 或其他后端句柄。

## 已确认设计

### 组件与边界

新增独立 `RenderGraph` component 和 `granit::render_graph` CMake target。它只链接核心
`granit::granit`，不链接 `granit::render_pipeline`、Scene 或 Material。公共入口为：

- `include/granit/render_graph.h`：C11 ABI、句柄、资源 ID、描述结构、回调和结果查询；
- `include/granit/render_graph.hpp`：C++20 RAII 包装和类型安全辅助；
- 安装 SDK 导出 `granit::render_graph`，外部项目只需 `find_package(granit COMPONENTS RenderGraph)`。

现有 `src/render_graph` 实现先拆分为公共实现和内部编译辅助；不得把 `src/render_graph/*.h`
作为安装头文件继续使用。

### 资源所有权

- 导入资源默认是借用，不由 Graph 销毁，也不因导入而延长句柄所有权；调用方必须保证资源在
  该次执行及 Renderer 的 GPU 使用期内有效。
- 临时资源由 Graph 创建。Graph 在录制完成并提交后才释放其句柄，底层 Renderer 负责把释放
  延迟到 GPU 不再使用之后；录制失败、提交失败和销毁路径都必须清理已创建资源。
- 回调取得的 Buffer、Texture View、Resource ID 和 Recorder 句柄只在当前回调期间有效，不能
  保存到回调之外；Pipeline 由调用方管理，Graph 不接管其生命周期。

### 图与 Pass 生命周期

- 创建和修改 Graph、编译 Graph、执行 Graph、销毁 Graph默认在同一拥有线程完成；首期不承诺
  并发修改或并行执行。
- Pass 名称、资源名称、Pass 描述、回调和用户数据在 Graph 执行完成前必须保持有效；C++ 包装
  可复制名称，但不复制用户数据。
- Graph 可以重复执行。每次执行读取当前外部句柄和临时资源描述；修改输出尺寸通过更新临时
  Texture 描述并重新编译，不能在录制回调中隐式重建资源。
- 编译失败不创建 GPU 资源；录制、创建或提交失败返回阶段、Pass、资源和依赖诊断，并释放本次
  执行已创建的临时资源。

### 访问声明与状态转换

每个 Pass 对同一资源只能声明一次，访问声明同时包含访问方向和用途：

- `READ`、`WRITE`、`READ_WRITE` 表示数据依赖；
- `SAMPLED` 表示着色器采样读取；
- `STORAGE` 表示 Storage/Unordered 读写；
- `COLOR_ATTACHMENT`、`DEPTH_ATTACHMENT` 表示动态渲染附件；
- `TRANSFER_SRC`、`TRANSFER_DST` 表示复制或清除用途。

Graph 使用这些声明建立依赖，并把后端状态转换交给 Renderer 的内部资源状态系统。调用方不
声明 Vulkan Image Layout，也不能假定某个后端的具体 Barrier。用途和访问方向不匹配、临时
资源未初始化读取、同一 Pass 重复声明和循环依赖都必须在编译阶段报告。

### Subpass 边界

首期 Graph 节点对应独立的可调度 Pass，不自动合并底层 Render Pass，也不模拟 Vulkan input
attachment。需要局部读取时，调用方应把前一 Pass 的输出声明为后续 Pass 的 sampled 输入，
必要时拆成独立动态渲染 Pass。一个回调可以连续记录多个命令阶段，但必须自行管理每次
`begin_rendering`/`end_rendering`，Graph 只负责该回调声明的资源状态和 Pass 依赖。

## 实施顺序

1. 从内部原型提取公共模型，定义 C ABI、C++ 包装、错误诊断、访问用途和生命周期契约；
2. 建立独立 `RenderGraph` component、安装导出和 C/C++ 头文件契约测试；
3. 实现 Graph Builder、资源导入、临时资源、Pass 声明、显式依赖和编译诊断；
4. 实现离屏执行、Frame 执行、回调资源查询和提交后临时资源回收；
5. 接入 Renderer 资源状态转换，并覆盖 attachment、sampled、storage、transfer 用途；
6. 增加循环依赖、未初始化读取、失败清理、重复执行和尺寸更新测试；
7. 增加只使用安装 SDK 的 `examples/standalone/render_graph` 示例，覆盖 Shadow → Scene →
   Tone Mapping 的最小流程，并分别验证离屏纹理和窗口 Frame 输出；
8. 更新公共 Reference、架构说明、版本文档和验收记录，完成 Windows/Linux/Emscripten 编译
   边界验证。

## 首期验收

- 外部示例只包含 `granit/*.h` 或 `granit/*.hpp`，只链接 `granit::render_graph` 与核心 Renderer；
- Shadow 写入、Scene 读取并写 HDR/Depth、Tone Mapping 读取 HDR 并写输出的图可编译和执行；
- 同一 Graph 能执行到离屏 Texture View 和已获取的窗口 Frame；
- 调整输出尺寸后，临时资源按明确的更新/重编译流程重建；
- 错误能定位到阶段、Pass 名称、资源名称和依赖关系；
- 临时资源不会在提交前销毁，失败退出和重复执行无句柄泄漏；
- 回调可以使用调用方准备好的 Pipeline、Buffer、Texture View 和资源绑定；
- 安装 Consumer 在干净构建目录中通过，不包含内部头文件路径。

## 风险与未决问题

- 当前 Renderer 的资源状态系统是否能覆盖 WebGPU 与 Vulkan 的统一用途映射，需要先做能力矩阵；
- 现有句柄销毁语义是否足以支撑 Graph 提交后的延迟回收，需要增加生命周期回归；
- 输出 Texture View 是否需要区分资源所有者和 View 所有者，避免导入 View 时误释放父 Texture；
- C ABI 回调异常不可跨边界传播，C++ 包装必须捕获异常并转为结果码；
- 在公共 ABI 稳定前，首期应标记为实验性 component，允许后续直接调整结构布局。
