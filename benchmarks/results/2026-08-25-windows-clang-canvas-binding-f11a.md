<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-25 Windows Clang Canvas Binding F-11A 基线

## 结论

Canvas 多纹理的主要成本来自录制期间反复结束 Dynamic Rendering、更新 Material 绑定并重新开始
Rendering。几何上传与 Queue submit 不是首要瓶颈，F-11 后续应以 CPU Record 和 GPU 时间作为核心
验收指标。

## 环境与命令

- Windows 10，Clang 22.1.8，Release 共享库。
- 分支基线：`feat/f11-canvas-binding-cache`。

```powershell
cmake --build build/windows-clang-release --target granit_canvas_gpu_benchmarks
.\build\windows-clang-release\bin\granit_canvas_gpu_benchmarks.exe
```

## 100 Item 结果

| 场景 | Batch | CPU Record | CPU Submit | Reset Wait | GPU |
|---|---:|---:|---:|---:|---:|
| 单纹理兼容 | 1 | 0.032 ms | 0.020 ms | 0.178 ms | 0.033 ms |
| 双纹理交替 | 100 | 0.954 ms | 0.037 ms | 1.307 ms | 0.810 ms |

交替纹理相对兼容路径使 CPU Record 放大约 30 倍、GPU 时间放大约 24 倍。Submit 增量很小；Reset
Wait 是同步等待 GPU 完成的结果，不能解释为 Queue API 本身成本。

## 指标说明

- CPU Record 只包围 `record_canvas_pass`。
- CPU Submit 只包围 Recorder submit 调用。
- Reset Wait 包围 Recorder reset，包含等待 GPU 完成。
- GPU 时间由命令流顶部和底部 Timestamp Query 得到。
- 数值为 15 组样本均值，用于同机改动前后对照，不代表跨设备绝对性能。
