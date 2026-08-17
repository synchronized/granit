<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ImGui Draw Data 转换首份 CPU 性能基线

## 环境

- 日期：2026-08-17
- 代码基线：`d0890b6`；测量工作区包含本记录对应的优化
- 系统：Windows AMD64
- CPU：Intel Core i5-8600K 3.60 GHz
- 编译器：MSVC 19.41.34123.0
- 构建：Release、共享库、ImGui 1.92.9
- 样本：20，预热：3，完整优化后运行重复 3 次

## P50 范围

| ImGui Draw Command | 优化前诊断值 | 优化后范围 |
| ---: | ---: | ---: |
| 10 | 2.077 µs | 1.504～1.648 µs |
| 100 | 90.453 µs | 12.794～13.554 µs |
| 1,000 | 11.067 ms | 124.280～125.070 µs |

基准固定每个命令包含一个四顶点、六索引矩形，Texture 与 Scissor 相同。计时包含清空 Canvas、
转换顶点与索引、解析 Texture ID、追加 Canvas Item 和查询统计，不包含 GPU 上传或录制。

## 发现与判断

- 初始实现为每个命令重复转换整个 ImGui Draw List 顶点数组，命令数量与顶点数量同步增长时形成
  二次复杂度；1,000 命令的 P50 已超过 11 ms。
- 改为只转换每个命令索引实际覆盖的连续顶点范围后，1,000 命令 P50 降至约 0.125 ms，约快
  88 倍，并保持命令顺序、Texture、Scissor 和相邻状态合批语义。
- 现有 Canvas CPU/GPU 基线已经覆盖动态几何上传与兼容/交替状态合批，没有证据需要为 ImGui
  增加专用上传器或破坏透明顺序的跨命令重排。
- 字体 Atlas 由应用拥有并通过现有 Texture 上传路径更新，不属于 Draw Data 转换组件。当前没有
  数据支持扩大多 Viewport 或 SDL Event 转换范围。

## 复测条件

- ImGui 顶点或索引布局、Draw Command 切分方式、Canvas 追加接口发生变化。
- 增加批量 Canvas 追加、专用 ImGui 上传器、多 Viewport 或用户 Draw Callback 支持。
- 同环境 P50/P95 相对本基线稳定退化超过 10%。
