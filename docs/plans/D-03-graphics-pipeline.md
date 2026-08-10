<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-03：Graphics Pipeline 与 Bind Group

## 元数据

- 设计状态：已确认
- 实现状态：未开始
- 路线图任务：D-03
- 优先级：P0
- 前置依赖：D-01、D-02、R-05、R-06、R-09、F-02
- 后续依赖：D-04、D-05、D-06、D-07

## 目标

建立不暴露 Vulkan Descriptor Set、Descriptor Pool 和 Pipeline 对象的公共渲染接口。D-03 分为
两个可独立验收的子任务：

- **D-03A**：先实现空资源布局的 Graphics Pipeline，打通 Shader 到最小绘制管线的创建路径。
- **D-03B**：再实现 Bind Group Layout、Pipeline Layout 和 Bind Group，提供稳定的资源绑定模型。

该顺序先验证图形管线、Dynamic Rendering 格式和固定功能状态，再引入资源绑定复杂度。

## 公共抽象

普通用户只接触 Granit 类型：

```text
Bind Group Layout ─┐
Bind Group         ├── Pipeline Layout ── Graphics Pipeline
Shader ────────────┘
```

- `Bind Group Layout` 描述一个组内各 binding 的资源类型、数组数量和可见 Shader 阶段。
- `Bind Group` 保存符合某个布局的 Buffer、Texture View 和 Sampler 绑定。
- `Pipeline Layout` 由有序的 Bind Group Layout 列表和后续 Push Constant 范围组成。
- `Graphics Pipeline` 保存 Shader、Pipeline Layout、Render Target 格式和固定功能状态。
- Command Recorder 后续按组绑定 Bind Group，不公开 Descriptor Pool 或 Descriptor Set 生命周期。

内部 Vulkan 映射可以使用 Descriptor Set Layout、Descriptor Set 和 Descriptor Pool，但这些名称、
类型及分配规则不进入公共 API。

## 已确认决策

### Bind Group 风格

- 公共 API 采用 Bind Group，而不是直接仿照 Vulkan Descriptor Set。
- 保留数值化 `group` 与 `binding`，便于离线 Shader 工具稳定映射 SPIR-V 资源位置。
- Texture View 与 Sampler 是独立绑定类型，不在公共模型中强制使用 combined image sampler。
- 第一版 Bind Group 创建后不可修改；资源变化时创建新 Bind Group。
- Descriptor Pool 由 Renderer 内部管理，用户不负责容量规划、重置或销毁。
- 第一版不提供 bindless、可变长度数组、update-after-bind 和动态 Buffer offset；根据实际性能需求
  在后续任务扩展。

不可变 Bind Group 能简化并发、生命周期验证和在途 GPU 使用；代价是高频变化资源需要后续增加
瞬态分配或动态偏移机制，不能依赖反复原地更新。

### Pipeline Layout

- Pipeline Layout 是显式对象，不由运行时反射隐式生成。
- D-03A 允许创建不含 Bind Group 和 Push Constant 的空 Pipeline Layout。
- Shader 反射用于离线校验和工具生成，不作为运行时创建布局的唯一事实来源。
- Bind Group Layout 在 Pipeline Layout 和 Bind Group 存活期间由内部强引用保持有效；销毁公开句柄
  不得导致已有依赖立即悬空。

### Graphics Pipeline

- 第一版采用独立 `granit_graphics_pipeline` 句柄，不使用含糊的通用 Pipeline 句柄。
- 必须显式提供 Vertex Shader、Fragment Shader、Pipeline Layout 和 Attachment 格式。
- Attachment 格式属于管线兼容性的一部分，与 Vulkan 1.3 Dynamic Rendering 对接。
- Viewport 和 Scissor 采用动态状态，在录制命令时提供，不烘焙进 Pipeline。
- 第一版只覆盖三角形列表、基础光栅化、颜色写入和可选深度测试所需的最小状态。
- Tessellation、Geometry Shader、多重颜色混合高级项和派生管线不进入 D-03A。

## API 与 ABI 约束

- 所有对象继续使用 64 位 generation 句柄，并校验资源类型和 Renderer domain。
- 创建描述包含 `struct_size`，枚举和标志均为 Granit 自有类型。
- 创建函数复制布局条目和绑定条目，不在返回后引用调用者数组。
- C++20 包装保持 move-only RAII，不建立第二套缓存或资源状态。
- API 不出现 `VkFormat`、`VkDescriptorType`、Shader Stage 位值或任何 Vulkan 头文件类型。

具体字段在各子任务实现前依据最小示例反推，并同步加入 C11 头文件编译测试，避免过早固化无用
状态组合。

## 生命周期与验证

- Shader、布局、绑定资源与 Pipeline 必须属于同一 Renderer。
- Bind Group 创建时校验 binding 完整性、重复项、类型、数组范围和资源 domain。
- Pipeline 创建时校验 Shader 阶段、布局、Attachment 格式及状态组合。
- Bind Group 内部持有绑定资源引用；公开资源句柄销毁后，已经创建的 Bind Group 仍能安全完成
已提交工作，但验证层应报告不推荐的父资源提前销毁行为。
- 销毁公开句柄后立即失效，底层 Vulkan 对象按照 R-08 的真实 GPU 完成点延迟退役。
- Renderer 级联销毁时报告仍存活的布局、Bind Group 和 Pipeline，并按依赖顺序回收。
- 创建与销毁允许从不同线程调用；同一公开对象不得与其销毁并发。

## 分步实施

### D-03A：最小 Graphics Pipeline

1. 定义空 Pipeline Layout 和 Graphics Pipeline 的 C API、C++ RAII 与句柄类型。
2. 实现内部 Pipeline Layout、Graphics Pipeline Registry 和 Vulkan 创建路径。
3. 接入 Dynamic Rendering 的颜色及可选深度格式。
4. 接入 R-08 延迟销毁、Device Lost 门禁和生命周期诊断。
5. 覆盖描述校验、跨 Renderer、错误 Shader 阶段和 generation 失效测试。

### D-03B：Bind Group

1. 定义 Bind Group Layout、Binding 类型和 Pipeline Layout 组合接口。
2. 实现内部 Descriptor Set Layout 与 Descriptor Pool 管理。
3. 实现不可变 Bind Group 创建、资源强引用和 Descriptor 写入。
4. 为 Command Recorder 增加批量 Bind Group 绑定命令。
5. 增加布局不兼容、缺失 binding、类型错误、跨 Renderer 和在途销毁测试。

## 验收标准

- 公共 C/C++ 头文件中不存在 Vulkan 类型或头文件依赖。
- D-03A 能用 Vertex/Fragment Shader 创建适用于 Dynamic Rendering 的最小 Graphics Pipeline。
- D-03B 能用独立 Buffer、Texture View 和 Sampler 组成不可变 Bind Group 并录制绑定。
- 所有新对象覆盖无效句柄、类型错误、generation、跨 Renderer、重复销毁和级联销毁。
- Windows Clang 与 Visual Studio 2022 的共享库测试通过，并至少验证一个静态库 preset。
- Vulkan Validation Layer 下创建、绑定、提交和销毁不产生生命周期或 Descriptor 错误。

## 后续扩展

- D-05 增加 Pipeline 绑定、Vertex/Index Buffer 与 Draw 命令。
- D-06 以最小三角形示例验证完整接口，而不是把示例专用逻辑放入 D-03。
- D-07 复用 Pipeline Layout 与 Bind Group 实现 Compute Pipeline。
- 根据性能数据评估动态 Buffer offset、瞬态 Bind Group、bindless 和 Push Constant。
- D-08 处理 Pipeline Cache、异步创建和热重载，不进入第一版同步创建接口。

