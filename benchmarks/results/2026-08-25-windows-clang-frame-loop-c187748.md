<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-12B Windows Clang 帧循环性能基线

## 结论

48 组逐帧原始样本确认，FIFO 的全帧 p50 约为 16.62～16.69 ms，其中 acquire p50 约为
16.02～16.48 ms。该路径受 59 Hz 显示节奏控制，继续优化 Canvas、Submit 或 Present 不会显著
提高 FIFO FPS。

Immediate、关闭 Validation 时，三至四帧槽的全帧 p50 为 0.36～0.62 ms，对应约
1,600～2,800 FPS。此前标题栏看起来“FPS 没有明显变化”不能代表引擎吞吐没有变化；应以固定采样
窗口的原始帧时间分布为准。

Validation 开启后，可控增量主要位于 Canvas Record 和 Submit。以自定义纹理负载为例，三槽的
Canvas Record p50 从 0.10 ms 增至 0.24 ms，Submit 从 0.07 ms 增至 0.16 ms。关闭 Validation
的生产路径中，两项均不是全帧主导成本，因此当前没有证据进入 F-12C 的通用 Recorder/提交改造。

## 环境与方法

- 日期：2026-08-25
- 提交：`c187748`
- 系统：Windows 10 AMD64
- GPU：Intel UHD Graphics 630
- 驱动：31.0.101.2140
- 显示：1920×1080，59 Hz；示例窗口固定为 1280×720
- 编译器：Clang 22.1.8
- 构建：`windows-clang-release`，共享库
- 每组：60 帧预热，300 帧原始样本
- 矩阵：Validation 开/关、Immediate/FIFO、1～4 帧槽、精简 UI/完整 Demo/自定义纹理
- 统计：每项独立计算 p50、p95、p99；GPU 空值不按零参与统计

原始 48 份 CSV 与汇总 CSV 保留在本地构建目录，本文件只保存足以支持决策的摘要。CPU 阶段使用
wall time；GPU Timestamp 单独报告，两者存在重叠，不能相加为总帧时间。

## Immediate 结果

以下为“完整 Demo + 自定义纹理”负载；单位均为毫秒。

| Validation | 帧槽 | 全帧 p50 | 全帧 p95 | Acquire p50 | Canvas p50 | Submit p50 | GPU p50 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 关 | 1 | 1.19 | 1.50 | 0.92 | 0.06 | 0.04 | 0.45 |
| 关 | 2 | 0.69 | 1.60 | 0.06 | 0.11 | 0.08 | 0.20 |
| 关 | 3 | 0.62 | 1.91 | 0.01 | 0.10 | 0.07 | 0.20 |
| 关 | 4 | 0.61 | 1.84 | 0.01 | 0.08 | 0.06 | 0.22 |
| 开 | 1 | 1.31 | 1.95 | 0.55 | 0.26 | 0.18 | 0.45 |
| 开 | 2 | 0.77 | 1.22 | 0.10 | 0.22 | 0.14 | 0.27 |
| 开 | 3 | 0.83 | 1.31 | 0.11 | 0.24 | 0.16 | 0.23 |
| 开 | 4 | 0.83 | 1.41 | 0.12 | 0.24 | 0.14 | 0.25 |

精简 UI 与无自定义纹理 Demo 得到相同方向：单槽由 acquire/GPU 背压主导；增加帧槽可降低 p50，
但 p95 不随槽数单调下降。四槽精简 UI、无 Validation 的最低全帧 p50 为 0.36 ms；这不是默认
采用四槽的充分依据，因为本轮没有输入延迟数据，且代表性负载的 p95 没有同步改善。

## FIFO 结果

全部 24 组 FIFO 配置的范围如下：

| 指标 | p50 范围 | p95 范围 |
|---|---:|---:|
| 全帧 | 16.62～16.69 ms | 17.94～24.35 ms |
| Acquire | 16.02～16.48 ms | 17.34～23.84 ms |

Present p50 仅约 0.01～0.03 ms，等待发生在 acquire，而不是 present 调用。该位置是当前 Vulkan
Swapchain/驱动的节流表现，不应通过跳过正确等待或增加提交排队深度规避。FIFO 用于验证帧节奏，
不用于观察引擎峰值吞吐。

## 归因与后续决策

1. **F-12C 暂不进入**：关闭 Validation 后 Canvas Record 与 Submit 绝对成本较小，没有达到通用
   架构改造的证据闸门。
2. **F-12D 暂不进入**：FIFO 等待属于显示节奏；Immediate 的 acquire p95 反映 GPU/驱动背压，
   增加帧槽只改变排队位置，不能视为消除成本。
3. **进入 F-12E**：对 Validation 下 Canvas Record 与 Submit 的增量做调用栈归因，只修复 Granit
   自身重复校验或重复查找，不把外部 Layer 固定成本当作缺陷。
4. 默认三帧槽保持不变；调整默认值前必须补充输入延迟或等价响应证据。

## 复现命令

```powershell
.\build\windows-clang-release\bin\granit_sdl3_imgui_example.exe `
  --present-mode immediate --frames-in-flight 3 --no-validation `
  --profile-warmup 60 --profile-frames 300 `
  --profile-output imgui-immediate-3-off.csv
```

完整矩阵只改变 `--present-mode`、`--frames-in-flight`、`--no-validation`、`--no-demo` 和
`--no-custom-texture`，其余条件保持一致。
