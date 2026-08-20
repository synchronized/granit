<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-09 高级渲染能力评估记录

## 结果

H-09 已完成固定种子负载、透明覆盖、阴影覆盖、多光源和绑定压力评估。四项候选能力必须独立满足
原型门槛；当前没有任何一项同时具备目标产品需求、多设备稳定证据、P50/P95 瓶颈归因和明确回退
边界，因此均暂缓原型，不改变默认渲染路径。

| 能力 | 当前决策 | 保留路径 | 主要依据 |
|---|---|---|---|
| 透明 PBR | 暂缓原型 | Unlit 透明与 Alpha Cutoff | 当前没有受光透明材质需求；现有覆盖成本主要来自 Draw 和绑定切换 |
| CSM | 暂缓原型 | 单方向光阴影图 | 已建立近/中/远 texel 密度测量，但没有目标场景的可见质量失败或预算依据 |
| Clustered Forward | 暂缓原型 | Forward 逐光路径 | 128 点光已在单台集成显卡形成瓶颈，但缺少目标分辨率、产品负载和多设备复测 |
| Bindless/Resource Table | 暂缓原型 | 传统 Bind Group | 8～512 纹理组未产生单调性能恶化，且 Shader 尚未采样压力纹理 |

“暂缓”只表示当前证据不足，不表示能力被否决。四项能力以后可以分别恢复，不需要等待其他三项。

## 证据

### 透明 PBR

[透明覆盖层基线](../../benchmarks/results/2026-08-18-windows-clang-transparent-b4b0396.md)
显示透明成本随实际 Draw 和绑定切换增加；兼容合批路径即使达到 32 层，64×64 GPU P50 仍约为
26.3 μs。现有 [透明 PBR 正确性契约](../plans/H-09B-transparent-pbr-correctness.md) 已固定未来原型
的 HDR 合成、稳定排序、深度、预乘 Alpha 和生命周期边界，但仓库没有已确认的受光透明材质需求。

决策为继续现有 Unlit 透明和 Alpha Cutoff。出现明确受光玻璃、粒子或植被需求，且现有路径无法
表达时，按正确性契约单独建立透明 PBR 原型 Plan。

### CSM

Render Pipeline benchmark 已固定 1024×1024 单阴影图，并提供总跨度 10、40、160 的近、中、远
档位，对应约 0.0098、0.0391、0.1563 世界单位/texel。该工具可以复现覆盖密度变化，但合成场景
没有证明单阴影图在目标摄像机路径中产生不可接受的闪烁、锯齿或远景丢失，也没有目标阴影预算。

决策为继续单方向光阴影图。只有目标场景同时提供固定摄像机路径、可比较截图或数值图像指标，且
单阴影图质量不足时，才按相同总阴影像素预算建立 CSM 原型 Plan。

### Clustered Forward

[多光源曲线](../../benchmarks/results/2026-08-19-windows-clang-multi-light-7e14162.md)显示 Intel UHD
Graphics 630 上 128 点光的 Opaque P50 约为 20.18 ms，占端到端 P50 约 86%；P50/P95 均随点光
数量近似线性增长。这证明当前逐光 Shader 在该合成负载和设备上可以成为主要瓶颈。

该结果仍只有 64×64 合成场景和单台集成显卡，未覆盖目标分辨率、典型材质、独立显卡或产品光源
分布。决策为继续 Forward 默认路径；目标场景稳定需要 64～128 个同时可见点光且在目标设备上重复
超出帧预算后，再建立保留 Forward 回退和像素对照的 Clustered Forward 原型 Plan。

### Bindless/Resource Table

[绑定压力曲线](../../benchmarks/results/2026-08-20-windows-clang-binding-pressure-7b3e246.md)固定
1,000 Draw 和 512 材质，将纹理组从 8 增至 512。材质资源更新 P50 维持约 6.9～7.1 μs，Opaque
P50 维持约 39.7～40.0 ms，没有随纹理组基数单调恶化。

当前 Shader 不采样压力纹理，512 材质也始终产生独立材质 Bind Group，因此该结果不能替代真实
Bindless 对照。决策为继续传统 Bind Group，并保持 [D-09 边界草案](../plans/D-09-bindless-resource-table.md)
未开始。只有真实 Shader 访问和资源分布证明创建、更新、切换或 Descriptor Pool 占用成为稳定
瓶颈时，才先进入纯 CPU Resource Table 原型。

## 验证边界

- 基准已在 Windows Intel Vulkan 硬件驱动完成；Windows/Linux CI 覆盖构建、测试和 Consumer，
  但不提供等价的跨 GPU 性能数据。
- 软件 Vulkan 驱动适合检查可复现性和正确性，不可替代目标硬件性能结论。
- 合成负载只建立可重复的基础曲线；重新立项必须补充真实项目数据、目标设备和明确预算。
- 四项原型不得互相捆绑，也不得污染 C ABI、公共头文件或现有回退路径。
