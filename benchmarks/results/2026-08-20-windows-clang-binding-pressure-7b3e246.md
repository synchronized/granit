<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-09E Render Pipeline 绑定压力曲线

## 环境与方法

- 提交：`7b3e246`（基准代码基于该提交的工作区构建）
- 系统：Windows 10 AMD64
- CPU：Intel Core i5-8600K，6 核 6 线程，3.60 GHz
- Vulkan 设备：Intel UHD Graphics 630，驱动 31.0.101.2140
- 编译器：Clang 22.1.8
- 构建：`windows-clang-release`，共享库，Validation 关闭
- 固定负载：64×64、1,000 Draw、512 材质、16 点光、中景阴影
- 变量：8、64、512 个纹理组，对应 7、63、511 次纹理组切换
- 参数：每档 2 次预热、10 个正式样本，每个样本 3 帧

每档创建 512 个真实材质 Bind Group，并对每个材质执行一次纹理资源更新。创建指标还包含归档
解码、Layout 和 Pipeline Template 初始化，因此只能作为 Bind Group 创建成本的保守上界。热路径
保留 2,000 个 Opaque/Shadow 逐 Draw 缓存条目和 1,000 个 Opaque 批次。

## 结果

单位均为微秒，GPU Opaque 和端到端行为毫秒。

| 纹理组 | 创建 P50 | 创建 P95 | 资源更新 P50 | 资源更新 P95 | Opaque P50 | Opaque P95 | 端到端 P50 | 端到端 P95 |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 130.2 μs | 199.1 μs | 7.1 μs | 8.3 μs | 40.012 ms | 40.963 ms | 85.625 ms | 102.727 ms |
| 64 | 125.9 μs | 176.8 μs | 6.9 μs | 7.8 μs | 39.703 ms | 41.765 ms | 79.946 ms | 98.924 ms |
| 512 | 123.5 μs | 168.7 μs | 7.0 μs | 7.8 μs | 39.782 ms | 41.104 ms | 80.227 ms | 88.549 ms |

## 结论

- 纹理组切换从 7 增至 511 时，材质创建、资源更新和 Opaque GPU 时间没有单调增长，当前结果
  不支持仅为纹理组数量引入 Bindless 或 Resource Table。
- 512 材质始终对应 512 个独立材质 Bind Group 和 511 次材质切换；改变纹理组不会减少当前材质
  Group 绑定。该曲线只隔离资源复用基数，不是“传统绑定与 Bindless”的完整对照。
- 当前 PBR Shader 不采样工作负载纹理，因此结果只覆盖描述符创建、资源变更、命令绑定和缓存占用，
  不覆盖纹理访问一致性、Descriptor Cache 或非一致索引成本。
- D-09 的真实 Shader 访问、目标内容资源分布和明确帧预算条件尚未满足，Bindless/Resource Table
  原型暂缓。后续只有在真实材质负载证明绑定成为 P50/P95 瓶颈时再建立独立原型 Plan。

## 复现命令

```powershell
foreach ($groups in 8, 64, 512) {
  ./build/windows-clang-release/bin/granit_render_pipeline_benchmarks.exe `
    --draws 1000 --materials 512 --texture-groups $groups --lights 16 `
    --shadow-range medium --iterations 3 --samples 10 --warmup 2
}
```
