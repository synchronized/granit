<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-11C Windows Clang Canvas 纹理绑定缓存结果

## 结论

Texture/Sampler Bind Group 缓存显著降低了交替纹理 Canvas 的 CPU 录制成本。100 个交替纹理
Batch 的 CPU Record 均值由 F-11A 的约 0.954 ms 降至约 0.243 ms，下降约 75%。本阶段仍会在
纹理切换时结束并重开 Dynamic Rendering，因此 GPU 与剩余 CPU 成本留给 F-11D 处理。

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
| 100 个兼容 Item | 1 | 0.012 ms | 0.033 ms |
| 100 个交替纹理 Item | 100 | 0.243 ms | 0.532 ms |
| 1,000 个交替纹理 Item | 1,000 | 2.277 ms | 5.569 ms |

交替场景只使用两个纹理组合。预热后两个 Bind Group 均命中缓存，因此测量主要反映缓存查询、绑定、
Rendering 区间切换和 Draw 成本，不再包含逐 Batch 的 Material 更新及 Bind Group 重建。

## 验证

- `cmake --build --preset windows-clang-debug`：通过。
- `ctest --preset windows-clang-debug`：47/47 通过。
- `cmake --build build/windows-clang-release --target granit_canvas_gpu_benchmarks`：通过。
- `granit_canvas_gpu_benchmarks.exe`：通过。
