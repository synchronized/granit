<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 07：迁移到 Render Pipeline

本章保留已有 Window、Mesh 和 Material，把手工 Pipeline 绑定与 Draw 迁移到 Granit 参考 Render
Pipeline。完成后，场景自动经过可见性、阴影、Forward PBR、HDR 和 Tone Mapping。

## 1. 创建 Scene Snapshot

每帧把 View、Renderable 和 Light 值数据提交为 Scene Snapshot。Renderable 的 `payload` 是上层分配的
关联键，用于下一步查找真正的 Mesh 和 Material；它不是 GPU 资源句柄。

Scene Snapshot 在创建时复制输入数组，因此局部数组随后可以释放。Mesh、Material 和其他 GPU 对象
由 Draw Binding 单独提供，并保持到渲染完成。

## 2. 建立 Draw Binding

为每个可绘制 payload 建立 `payload → Mesh + Material` 映射。缺少映射的 Renderable 不会自动猜测
资源；上层应把缺失绑定视为场景准备错误或明确跳过。

## 3. 执行参考管线

单 View 渲染描述提供 Scene Snapshot、输出 Backbuffer View、尺寸和 Draw Binding。默认管线依次执行：

```text
Scene Snapshot → Visibility → Shadow → Forward PBR HDR → Tone Mapping → Backbuffer
```

窗口没有可见物体时仍应完成清屏、Overlay 和帧提交。不要因为可见列表为空而跳过整个 Render
Pipeline 调用。

## 4. 验收

- 模型保持上一章材质外观，并新增方向光阴影。
- 没有可见物体时仍显示稳定清屏颜色。
- Resize 后 HDR/LDR 中间资源与输出尺寸同步。
- 多 View 或缺失 Draw Binding 不会混用其他 Renderer 的资源。

开发仓库还可以运行 `granit.pipeline.render` GPU 集成测试核对参考管线基础路径：

```powershell
ctest --preset windows-clang-debug -R "^granit\.pipeline\.render$" --output-on-failure
```

输入、所有权和扩展回调见 [Render Pipeline](../reference/render-pipeline.md)、
[Scene Snapshot](../reference/scene-snapshot.md)和 [Material](../reference/material.md)。下一章在最终输出上
叠加 ImGui 调试界面。

[上一章：材质与光照](06-material-and-lighting.md) · [下一章：接入 ImGui](08-imgui.md)
