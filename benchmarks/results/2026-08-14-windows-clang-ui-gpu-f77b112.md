<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# UI Pass 首份 GPU 性能基线

## 环境

- 日期：2026-08-14
- Granit：`f77b112`
- 系统：Windows AMD64
- CPU：Intel Core i5-8600K 3.60 GHz
- 编译器：Clang 22.1.8
- 构建：Release、共享库
- 目标：64×64 RGBA8 UNORM，预乘 Alpha 混合
- 样本：15，预热：3，完整运行重复 3 次

## P50 范围

| 矩形数 | 相邻状态全部兼容 | Texture/Scissor 逐项交替 | 交替路径约为兼容路径 |
| ---: | ---: | ---: | ---: |
| 100 | 40.050～40.308 µs | 0.811～0.817 ms | 20 倍 |
| 1,000 | 206.182～206.558 µs | 8.132～8.220 ms | 40 倍 |
| 10,000 | 1.864～1.897 ms | 81.309～82.837 ms | 44 倍 |

两条路径使用相同顶点数、索引数、目标尺寸和小面积重叠矩形。兼容路径将 N 个 Item 合为一个
Indexed Draw；交替路径由于 Texture 与 Scissor 每项改变，必须保留 N 个 Batch、绑定更新和 Draw。
时间来自 UI Pass 前后的 Vulkan timestamp，不包含 Draw List 构建、几何上传、CPU 录制、提交或等待。

## 判断

- 兼容路径近似随顶点和索引数量增长；10,000 矩形仍低于 2 ms，首版共享几何方案成立。
- 逐项交替路径主要暴露大量 Draw 与绑定切换成本，不代表典型 UI，但明确证明上层应保持相邻
  兼容项，并避免为每个字形、图标或 ImGui 命令无条件更换 Texture、Sampler 或 Scissor。
- 当前不立即引入 Bindless。该基线同时改变 Texture 和 Scissor，即使使用 Bindless，Scissor 边界
  仍会阻止合批；应先用真实 UI/ImGui 工作负载统计 Batch 数与状态分布。
- 当前每 Batch 通过 Material 更新重建 Bind Group，GPU timestamp 不包含这部分 CPU 成本。后续
  CPU 端到端测量若证明绑定更新显著，再引入按 Texture/Sampler 组合缓存的 Bind Group。

## 重评条件

- 典型帧稳定超过 1,000 个 UI Batch，或 UI GPU 时间稳定超过目标帧预算的 10%。
- 相同 Scissor 下仅因 Texture 变化产生大量 Batch，此时重新评估 Texture Array、Atlas 或 Bindless。
- 引入 Bind Group 缓存、持久映射、实例化或间接绘制后复测。
- 目标 GPU、分辨率、覆盖率、混合模式或纹理格式改变后，不直接沿用本机绝对时间。
