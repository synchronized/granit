<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-11D Windows Clang Canvas 单 Rendering 区间结果

## 结论

Canvas 在开始 Dynamic Rendering 前准备全部本帧纹理绑定，录制期间不再因纹理切换结束并重开
Rendering。100 个交替纹理 Batch 的 GPU 均值由 F-11C 的约 0.532 ms 降至约 0.068 ms，下降
约 87%；CPU Record 均值由约 0.243 ms 降至约 0.109 ms，下降约 55%。

## 环境

- 日期：2026-08-25
- 系统：Windows AMD64
- 编译器：Clang 22.1.8
- 构建：`windows-clang-release`，共享库
- GPU 时间戳：开启
- 样本：15 组；100 Item 场景每组 10 次迭代

## 关键结果

| 场景 | Batch | CPU Record 均值 | GPU 均值 |
|---|---:|---:|---:|
| 100 个兼容 Item | 1 | 0.016 ms | 0.033 ms |
| 100 个交替纹理 Item | 100 | 0.109 ms | 0.068 ms |
| 1,000 个交替纹理 Item | 1,000 | 1.124 ms | 0.507 ms |
| 10,000 个交替纹理 Item | 10,000 | 9.554 ms | 5.732 ms |

100 Batch 场景已低于 F-11 计划的 0.20 ms GPU 目标。大量 Batch 下仍呈近似线性增长，主要成本
转为绑定组切换、Scissor 与 Draw 调用数量，不再包含 Rendering 区间反复切换。

## 实现验证

- 全帧只有一次 `begin_rendering` 与一次 `end_rendering`。
- Rendering 前预绑定全部唯一 Material Bind Group，提前完成纹理状态准备。
- 相同布局、阶段和访问类型的图像状态跳过重复 Barrier。
- 超过 64 个唯一组合时允许帧内临时增长；录制完成后回收到 64 个持久缓存项。

## 验证命令

- `cmake --build --preset windows-clang-debug`：通过。
- `ctest --preset windows-clang-debug`：47/47 通过。
- `cmake --build build/windows-clang-release --target granit_canvas_gpu_benchmarks`：通过。
- `granit_canvas_gpu_benchmarks.exe`：通过。
