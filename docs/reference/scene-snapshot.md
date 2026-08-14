<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Scene Snapshot

Scene Snapshot 是提交给参考 Render Pipeline 的只读场景快照。它包含 View、可渲染项和光源的
值数据，但不拥有 ECS、Scene Graph、Mesh、Material 或资产数据库。

## 公共入口

- C：`<granit/pipeline/scene.h>`。
- C++20：`<granit/pipeline/scene.hpp>`，使用 move-only 的 `granit::scene_snapshot`。
- 所属 CMake component：`RenderPipeline`，目标为 `granit::render_pipeline`。

创建描述使用 `GRANIT_SCENE_SNAPSHOT_DESC_INIT` 初始化，并可提供：

- 一个或多个 View，包括矩阵、摄像机位置、viewport 和 layer mask。
- Renderable，包括变换、包围球、排序键、对象 ID 和不透明 `payload`。
- 方向光、点光和聚光灯值数据。

`payload` 由上层应用定义。Render Pipeline 通过它把 Renderable 映射到 Mesh 和 Material，不应
把它解释为指针或可持久化资源句柄。

## 复制与所有权

- 创建调用验证并复制所有数组和值数据。
- 调用返回后，输入数组无需继续保留。
- Snapshot 不持有外部对象引用，也不会销毁 Mesh、Material 或上层场景对象。
- 销毁后旧句柄立即失效；句柄只能与创建它的 Renderer 一起使用。

## 多 View 与可见性

每个 View 独立保存 viewport 和 layer mask。Renderable 和光源也具有 layer mask，参考管线据此
生成每个 View 的可见集合。`first_view` 和 `view_count` 由渲染调用选择 Snapshot 中的连续 View。

## 线程安全

Snapshot 创建后不可变，可被读取。不要让销毁与使用该 Snapshot 的渲染调用并发执行。

