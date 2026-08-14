<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Mesh

Mesh 描述一次不可变的绘制，包括图元拓扑、Vertex/Index Buffer、顶点布局和绘制范围。
它不保存资产文件、CPU 顶点数据或场景变换。

## 公共入口

- C：`<granit/pipeline/mesh.h>`，使用 `granit_mesh_create` 和 `granit_mesh_destroy`。
- C++20：`<granit/pipeline/mesh.hpp>`，使用 move-only 的 `granit::mesh`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

创建描述使用 `GRANIT_MESH_DESC_INIT` 初始化，再填写：

- `topology`：图元拓扑。
- `vertex_buffers`：一个或多个 Vertex Buffer 及对应布局。
- `indexed` 与 Index Buffer 字段：是否使用索引绘制及索引范围。
- Vertex、Index 和 Instance 的起始位置与数量。

## 所有权与生命周期

- Mesh 复制创建描述中的布局和绘制范围。
- Mesh 只借用 Vertex/Index Buffer，不负责销毁它们。
- 被借用的 Buffer 必须属于同一 Renderer，并在 Mesh 被使用期间保持有效。
- 销毁 Mesh 后旧句柄立即失效；重复销毁或跨 Renderer 使用返回无效句柄错误。

## 验证规则

- 至少提供一个 Vertex Buffer，当前最多提供 16 个。
- 每个顶点布局必须具有非零 stride 和至少一个属性。
- 同一个 Mesh 中的属性 location 不得重复，属性范围不得越过 stride。
- Buffer 必须具有对应的 Vertex 或 Index usage，且容量覆盖声明的绘制范围。
- 非索引绘制要求非零 `vertex_count`；索引绘制要求有效的 Index Buffer 和非零
  `index_count`。

## 线程安全

Mesh 创建后不可更新。不要让销毁与使用该 Mesh 的渲染调用并发执行。

