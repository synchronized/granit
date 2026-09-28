<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 06：GPU Marching Cubes

本教程使用 Compute Shader 从动画 Metaballs 密度场提取显式三角形网格，再通过 Indirect Draw
直接绘制 GPU 生成的顶点。完整源码位于
[`examples/tutorials/06_marching_cubes`](../../examples/tutorials/06_marching_cubes)。

## GPU 数据流

每帧通过 Frame Context 分配密度场、顶点和生成状态三个 Transient Buffer Slice。CPU 只更新参数，
几何数量和顶点内容都由 GPU 产生：

```text
Density Compute → Polygonize Compute → Finalize Compute
       密度场          顶点与计数          Draw Indirect 参数
                                             ↓
                                  Vertex Buffer + Draw Indirect
```

Polygonize Pass 使用 Atomic Counter 为每个 Cell 预留输出区间；超出容量时只设置 Overflow 标志，
不会越界写入。Finalize Pass 把有效顶点数钳制到完整三角形，并写入固定布局的
`draw_indirect_args`。Recorder 在 Vulkan 上建立 Storage Write 到 Vertex/Indirect Read 的屏障，
WebGPU 通过命令顺序和 Pass 边界表达相同依赖。

统计数据先复制到按 Frame Slot 保存的设备 Buffer，再通过 Async Readback 获取。读回只更新面板和
浏览器验收特性值，绘制路径不等待 CPU。支持 Timestamp Query 的后端显示 Density、Polygonize、
Finalize 和 Draw 时间；浏览器不支持当前 Timestamp 契约时明确显示不可用。

## 与 SDF Raymarch 的区别

[05：Metaballs](05-metaballs.md) 在 Fragment Shader 中沿射线求隐式曲面，不产生网格。
Marching Cubes 会生成真实顶点，因而能验证 Compute、Atomic、Transient Buffer、跨阶段同步和
Indirect Draw，但其内存与计算开销随三维网格分辨率增加。两章使用相同的球数量、半径、场强、
动画和轨道相机交互，便于比较两种路径。

三角形查表来自固定版本的 VTK Marching Cubes cases，许可证和来源提交记录在
[`THIRD_PARTY_NOTICES.md`](../../examples/tutorials/06_marching_cubes/THIRD_PARTY_NOTICES.md)。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_06_marching_cubes
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.06_marching_cubes$" `
  --output-on-failure
```

Smoke 模式固定时间和参数。浏览器验收检查 GPU 产生非零几何、多帧推进、Canvas 内容、Resize 恢复
及 WebGPU Validation 消息。
