<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-02 S-13G 模型查看器性能基线

## 范围

本记录验证桌面模型查看器的固定性能采样模式，不作为跨设备性能承诺。测试使用 Windows 10、
Clang Release、Intel UHD Graphics 630、Vulkan、FlightHelmet 和 1920×1080 窗口；关闭
Validation，预热 300 帧后采样 1000 帧。

## 结果

所有数值单位均为毫秒。CPU、GPU、帧槽等待和 Present 等待分别统计，不进行相加。

| 呈现模式 | UI | CPU p50/p95/p99 | GPU p50/p95/p99 | 帧槽等待 p95 | Present 等待 p95 |
| --- | --- | --- | --- | --- | --- |
| Immediate | 关闭 | 3.36 / 4.24 / 6.13 | 1.70 / 2.30 / 2.42 | 0.01 | 0.02 |
| Immediate | 开启 | 3.64 / 4.63 / 6.42 | 1.70 / 2.18 / 2.33 | 0.01 | 0.02 |
| FIFO | 关闭 | 17.02 / 32.16 / 36.23 | 2.47 / 2.57 / 2.69 | 24.85 | 0.04 |
| FIFO | 开启 | 16.82 / 19.17 / 30.19 | 2.46 / 2.55 / 2.69 | 13.29 | 0.04 |

四组 CPU、等待和 GPU 指标均包含 1000 个有效样本。Immediate 下 UI 路径使 CPU p50 增加约
0.28 ms；FIFO 的 CPU 墙钟主要受帧槽等待影响，因此不能据此判断 UI 更快。GPU 时间在四组中
保持同一量级，未发现 UI 引入不可解释的 GPU 退化。

## 验证

- Release 模型查看器与桌面参数测试构建通过。
- 桌面参数测试与文档检查通过。
- Vulkan UI Smoke Test 使用完整 FlightHelmet 通过。
- 四组 JSON 均记录实际后端、Adapter、资产、分辨率、呈现模式、UI、Validation 和样本数。

桌面 Dawn D3D12 与 Linux Dawn Vulkan 尚未在本机采样；需要在锁定 Dawn SDK 的手动验收环境中
使用相同参数补齐，不能将本机 Vulkan 数值作为 Dawn 或其他硬件的门槛。
