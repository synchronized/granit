<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Render Pipeline 与 H-05 性能对比

## 结论

当前统一 Render Pipeline 尚未通过 H-07H 性能验收。完成第一轮资源复用后，自动路径 CPU P50
由 2.907 ms 降至 1.518 ms，下降约 47.8%；最小回调路径保持在 0.629 ms。剩余约 0.890 ms
增量位于每帧灯光与阴影常量更新、Shadow Draw 和 Opaque Draw，仍需等工作量 CPU/GPU 复测。

## 环境与方法

- 基准代码：基于提交 `5c0613a` 的工作区。
- 系统：Windows AMD64。
- 编译器：Clang 22.1.8。
- 构建：Release，共享库。
- 参数：5 个预热样本，20 个正式样本，每个样本 20 帧。
- 统一路径：64×64，一个三角形、一盏方向光、真实 1024×1024 Shadow Draw、PBR 与 Tone Mapping。
- 手工路径：256×256，一个全屏三角形、一盏方向光与一个点光、1×1 Shadow Clear、PBR 与
  Tone Mapping。
- CPU 区间均包含录制、提交、等待；不包含初始化和像素回读。
- H-05 GPU 时间来自 Vulkan timestamp query，不包含 CPU 录制和等待。

两条路径的输出链相同，但 Shadow 和分辨率并非严格等工作量。本结果用于发现门面开销风险，不能
作为 Shader 吞吐量的逐项等价比较。

## 结果

单位为毫秒。

| 路径 | 范围 | Mean | P50 | P95 | P99 |
| --- | --- | ---: | ---: | ---: | ---: |
| 自动 Render Pipeline | CPU 端到端 | 3.005 | 2.907 | 3.263 | 4.435 |
| 最小回调 Render Pipeline | CPU 端到端 | 0.639 | 0.624 | 0.728 | 0.757 |
| 优化后自动 Render Pipeline | CPU 端到端 | 1.521 | 1.518 | 1.602 | 2.160 |
| 优化后最小回调 Render Pipeline | CPU 端到端 | 0.646 | 0.629 | 0.806 | 0.843 |
| 手工 H-05 | CPU 端到端 | 0.671 | 0.662 | 0.769 | 0.791 |
| 手工 H-05 | GPU Shadow | 0.004 | 0.004 | 0.004 | 0.004 |
| 手工 H-05 | GPU PBR HDR | 0.138 | 0.138 | 0.141 | 0.142 |
| 手工 H-05 | GPU Tone Mapping | 0.080 | 0.079 | 0.085 | 0.088 |
| 手工 H-05 | GPU 渲染链 | 0.223 | 0.221 | 0.227 | 0.229 |

## 判断与下一步

- 最小回调路径与手工 H-05 的 CPU P50 相差约 6%，说明 Graph、瞬态附件、Tone Mapping 和统一
  提交外壳不是 4 倍级差异的主要来源。
- 第一轮优化复用每个 Draw 的 Frame/Object Uniform Buffer、Bind Group 和 Shadow/IBL 光照资源，
  Uniform Buffer 使用可直接更新的 Upload 内存，并复用固定 1024×1024 阴影附件。
- 自动路径 P50 下降约 1.389 ms，说明每帧创建和销毁绑定资源是主要可消除开销。
- 优化后自动路径相对最小回调仍增加约 0.890 ms。下一轮应分别测量灯光与阴影常量更新、真实
  Shadow Draw 和 Opaque Draw，避免继续凭总时间猜测。
- H-05 的 Shadow 仅清除 1×1 深度目标，而自动路径执行真实 1024×1024 Shadow Draw；下一轮应
  建立严格等工作量用例。
- 统一门面完整 GPU 时间需要内部测试专用测量钩子或外部 GPU profiler。不得为基准扩张公共 ABI。
- 下一步建立严格等工作量用例并补充阶段级 GPU timestamp；若常量更新仍明显，再设计按 Frame
  生命周期管理的 Upload 环形分配，避免在尚未完成的 GPU 帧上原地覆写。
- 优化目标先定为同条件 CPU P50 不高于最小回调路径 1.5 倍加真实 Draw/GPU 等待成本；若无法达到，
  应记录不可消除成本和原因。
