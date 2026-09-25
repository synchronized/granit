<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-25 S-70 特性教程能力审计

本记录汇总 03 Instancing、04 Raymarch 与 05 Metaballs 的实现结果，用于判断后续公共 API 工作。当前
接口行为以 Reference 和源码为准，本记录不构成兼容承诺。

| 能力 | 结论 | 证据与后续处理 |
|---|---|---|
| 实例 Vertex Buffer 与实例 Draw | 现有 API 足够 | 45 个实例通过一次 `draw_indexed` 提交 |
| 无 Vertex Buffer 的全屏三角形 | 现有 API 足够 | `SV_VertexID` 与 `draw(3)` 无需专用全屏接口 |
| 动态 Uniform 与 Frame Slot 隔离 | 现有 API 足够 | 三个教程均通过对齐偏移更新逐帧数据 |
| HLSL-first 跨后端 Shader | 现有工具链足够 | 同一输入生成 SPIR-V 与 WGSL，并按逻辑名称加载 |
| SDF、平滑并集和法线估计 | 教程局部逻辑 | 属于效果数学，不进入 Renderer 或 Math 公共 API |
| 全屏效果公共基类 | 暂不需要 | 两个教程只有少量相似代码，抽象后会隐藏教学核心 |
| Indirect Draw/Dispatch | 后端能力缺口 | Buffer 已有 indirect usage，Recorder 尚无跨后端命令；另立计划 |
| Compute Metaballs/Marching Cubes | 暂缓 | Fragment 路径已满足教学与能力验证，待性能证据再规划 |

本轮没有发现必须立即扩展 C ABI 或 C++ 包装的阻塞项。下一批教程可以继续优先验证 Compute、
Indirect、Render-to-texture 或粒子等能力；其中 Indirect 应先完成独立的跨后端语义、能力查询和测试
设计。

