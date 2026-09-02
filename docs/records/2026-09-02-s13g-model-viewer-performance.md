<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-02 S-13G 模型查看器性能基线

## 本机 Vulkan 范围

本记录验证桌面模型查看器的固定性能采样模式，不作为跨设备性能承诺。测试使用 Windows 10、
Clang Release、Intel UHD Graphics 630、Vulkan、FlightHelmet 和 1920×1080 窗口；关闭
Validation，预热 300 帧后采样 1000 帧。

## 本机 Vulkan 结果

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

## Dawn 托管 Runner 结果

锁定 Dawn SDK 的手动验收分别在 GitHub Windows Runner 的 D3D12 后端和 Ubuntu 24.04
Runner 的 Lavapipe Vulkan 后端运行。Linux 窗口路径使用 Weston headless Wayland。两端均采用
Release、1920×1080、关闭 Validation、预热 300 帧并采样 1000 帧。

| 平台 | 呈现模式 | UI | CPU p50/p95/p99 | 帧槽等待 p95 | Present 等待 p95 |
| --- | --- | --- | --- | --- | --- |
| Windows Dawn | FIFO | 关闭 | 96.26 / 111.98 / 131.19 | 109.91 | 0.36 |
| Windows Dawn | FIFO | 开启 | 16.03 / 16.60 / 31.68 | 15.09 | 0.17 |
| Windows Dawn | Immediate | 关闭 | 94.13 / 95.24 / 96.90 | 92.86 | 0.16 |
| Windows Dawn | Immediate | 开启 | 15.99 / 16.51 / 17.48 | 15.06 | 0.22 |
| Linux Dawn | FIFO | 关闭 | 62.20 / 65.01 / 67.00 | 0.05 | 0.14 |
| Linux Dawn | FIFO | 开启 | 25.07 / 25.30 / 25.43 | 0.07 | 24.20 |
| Linux Dawn | Immediate | 关闭 | 62.10 / 64.70 / 67.05 | 0.05 | 0.13 |
| Linux Dawn | Immediate | 开启 | 1.77 / 5.13 / 5.25 | 4.30 | 0.13 |

Dawn Provider 在这些环境中未提供 GPU Timestamp，因此 GPU 样本数为零，不能把报告中的零值解释为
0 ms。托管 Runner 的共享资源、软件适配器和 headless compositor 会显著影响等待分布；UI 开启后
CPU 时间反而降低也说明这些数值只用于建立可复现采样链路，不用于比较 UI 成本或设定 FPS 门槛。

## 验证

- Release 模型查看器与桌面参数测试构建通过。
- 桌面参数测试与文档检查通过。
- Vulkan UI Smoke Test 使用完整 FlightHelmet 通过。
- 四组 JSON 均记录实际后端、Adapter、资产、分辨率、呈现模式、UI、Validation 和样本数。
- Windows Dawn 的构建、完整 FlightHelmet 截图与四组性能采样在
  [Dawn Integration #33591067006](https://github.com/synchronized/granit/actions/runs/33591067006)
  通过。
- Linux Dawn 的构建、完整 FlightHelmet 截图与四组性能采样在
  [Dawn Integration #33599195911](https://github.com/synchronized/granit/actions/runs/33599195911)
  通过；截图相对 Vulkan 参考图只有 2 个孤立轮廓残差，颜色 MAE 为 0.00264489。

本机 Vulkan、Windows Dawn D3D12 与 Linux Dawn Vulkan 的采样链路均已闭合。仍不能把任一环境
的数值外推为其他 GPU、驱动、窗口系统或浏览器的性能门槛。
