<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# PBR Pass 首份 CPU 性能基线

## 环境

- 日期：2026-08-12
- Granit：`6f9bf01`
- 系统：Windows AMD64
- CPU：Intel Core i5-8600K 3.60 GHz
- 编译器：Clang 22.1.8
- 构建：Release、共享核心库、静态 PBR 与 Render Graph 模块
- 每个 Pass：100 个 Object
- 每样本迭代：10,000 次
- 预热：5 次
- 样本：30 次

## 原始摘要

| 用例 | 平均值 | P50 | P95 | P99 |
| --- | ---: | ---: | ---: | ---: |
| `pbr_graph_build` | 7.436 us | 7.402 us | 7.573 us | 7.591 us |

该用例每次创建串行 Graph，导入一个颜色 Texture View，复制并打包 View、方向光和 100 个 Object，
加入一个 PBR Pass 并编译图。它不创建 Vulkan Renderer，不包含 GPU 命令录制、提交和实际 Draw。

## 判断与复测条件

- 该成本属于逐帧重建图时的 CPU 成本；长期复用 Graph 时不会逐 Draw 重复支付。
- 当前没有依据引入并行打包或对象常量缓存，先保留简单、明确的值语义。
- 对象常量布局、批量打包、Pass 捕获方式或 Render Graph 存储结构改变时复测。
- 同环境 P50 或 P95 相对本基线稳定退化超过 10% 时调查原因。
- 增加 Texture Readback 后另建 GPU 像素回归与重复帧基线，不与本 CPU 数据混合比较。
