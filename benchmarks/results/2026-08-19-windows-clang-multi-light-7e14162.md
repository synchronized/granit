<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-09D Render Pipeline 多光源曲线

## 环境与方法

- 提交：`7e14162`（包含 Render Pipeline 128 点光容量修复的工作区）
- 系统：Windows 10 AMD64
- CPU：Intel Core i5-8600K，6 核 6 线程，3.60 GHz
- Vulkan 设备：Intel UHD Graphics 630，驱动 31.0.101.2140，Vulkan 1.3.215
- 编译器：Clang 22.1.8
- 构建：`windows-clang-release`，共享库，Validation 关闭
- 负载：64×64 输出、100 Draw、8 材质、8 纹理组、1 盏方向光、中景 1024×1024 阴影
- 参数：每档 5 个预热样本、20 个正式样本，每个样本 20 帧

每档只改变点光数量。`gpu_opaque` 来自 Vulkan timestamp，不包含 CPU 等待；端到端指标包含场景
复制、Graph 构建、资源准备、命令录制、提交和完成等待。

## 结果

单位均为毫秒。表中只保留 H-09D 的决策指标；CSV 还同时输出 Shadow、Tone Mapping 和对照路径。

| 点光数 | Opaque P50 | Opaque P95 | 端到端 P50 | 端到端 P95 |
|---:|---:|---:|---:|---:|
| 1 | 1.868 | 2.700 | 8.460 | 12.202 |
| 16 | 4.034 | 4.368 | 9.588 | 11.511 |
| 64 | 10.930 | 11.361 | 18.717 | 79.772 |
| 128 | 20.176 | 20.299 | 23.582 | 26.176 |

64 点光端到端 P95 出现一次明显的 CPU 等待长尾，但同档 Opaque P95 稳定，不能把该长尾归因于
Shader。Shadow P50 在四档均约为 0.45 ms，符合只改变点光数量的预期。

## 结论

- Opaque P50/P95 随点光数量近似线性增长；128 点光 Opaque P50 约占端到端 P50 的 86%，当前
  Forward Shader 的逐光遍历已在这台集成显卡的合成负载中成为主要瓶颈。
- 128 点光首次运行暴露自动路径仍按默认 64 点光分配 Buffer；底层上限为 256，修复后打包与逐
  Draw 光照资源统一使用 128 容量，未改变公共 API。
- 该证据只覆盖 64×64 合成负载和单台集成显卡，当前仍缺少目标产品场景、目标分辨率与独立显卡
  复测，因此暂不建立 Clustered Forward 原型 Plan。
- 若产品需要 64～128 个同时可见点光，应在目标设备和分辨率复用本命令；P50/P95 仍超预算时，
  再建立带现有 Forward 回退和像素正确性对照的独立原型 Plan。

## 复现命令

```powershell
foreach ($lights in 1, 16, 64, 128) {
  ./build/windows-clang-release/bin/granit_render_pipeline_benchmarks.exe `
    --draws 100 --materials 8 --texture-groups 8 --lights $lights `
    --shadow-range medium --iterations 20 --samples 20 --warmup 5
}
```
