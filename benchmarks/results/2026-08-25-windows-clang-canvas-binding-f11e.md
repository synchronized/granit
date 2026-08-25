<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-11E Windows Clang ImGui 验证记录

## 结论

完整 ImGui Demo、自定义 Texture ID、裁剪和资源 generation 重建均验证通过。三槽仍是当前机器上
吞吐、槽等待与 CPU Record 的较佳平衡。与 F-10 同构的单字体场景中，无 Validation 的 Canvas
Record 由约 0.104 ms 降至约 0.040 ms，下降约 61%；Validation 下由约 0.353 ms 降至约
0.275 ms，下降约 22%，未达到计划的 30% 目标。

## 环境与方法

- 日期：2026-08-25
- 系统：Windows AMD64
- 编译器：Clang 22.1.8
- 构建：`windows-clang-release`，共享库
- 窗口：1280×720，Immediate Present，ImGui Demo 开启
- 每组：240 帧；输出为示例平滑后的末帧指标

## 1～4 帧槽与自定义纹理

默认示例同时显示字体 Atlas 和程序生成的 2×2 检查纹理，Validation 关闭。

| 帧槽 | CPU | GPU | 槽等待 | Acquire | Canvas Record | Submit |
|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.333 ms | 0.457 ms | 0.016 ms | 0.916 ms | 0.083 ms | 0.054 ms |
| 2 | 1.044 ms | 0.343 ms | 0.014 ms | 0.578 ms | 0.098 ms | 0.064 ms |
| 3 | 0.798 ms | 0.246 ms | 0.016 ms | 0.345 ms | 0.087 ms | 0.067 ms |
| 4 | 0.933 ms | 0.269 ms | 0.020 ms | 0.447 ms | 0.119 ms | 0.092 ms |

这些数据包含不同时间运行的窗口与驱动波动，不用于推导精确帧槽缩放比例；它们确认四种槽数都能
稳定完成字体和自定义纹理录制，没有同步或生命周期错误。

## 同构基线与 Validation

使用 `--no-custom-texture` 保持与 F-10 单字体完整 Demo 基线一致，帧槽数为 3。

| Validation | CPU | GPU | Canvas Record | Submit |
|---|---:|---:|---:|---:|
| 关闭 | 0.793 ms | 0.334 ms | 0.040 ms | 0.032 ms |
| 开启 | 0.953 ms | 0.310 ms | 0.275 ms | 0.214 ms |

Validation 的 CPU Record 与 Submit 固定成本仍较明显。该成本不影响 F-11D 已达到的多纹理 GPU
目标，但需要作为通用 Renderer/Validation 诊断项单独处理。

## 功能与生命周期覆盖

- 字体 Atlas 使用 Texture ID 1，自定义检查纹理使用 Texture ID 2。
- ImGui 转换测试覆盖 DisplayPos、FramebufferScale、两个 Texture ID 和独立 Scissor。
- Canvas 像素测试覆盖多纹理透明顺序及裁剪。
- 销毁缓存曾引用的 Texture View 后重新创建资源，新 generation 可正常录制。
- Windows Clang Debug 全部 47 项测试通过，包括 Vulkan Validation 窗口冒烟测试。

## 复现命令

```powershell
cmake --build build/windows-clang-release --target granit_sdl3_imgui_example
./build/windows-clang-release/bin/granit_sdl3_imgui_example.exe `
  --frame-count 240 --frames-in-flight 3 --no-validation
./build/windows-clang-release/bin/granit_sdl3_imgui_example.exe `
  --frame-count 240 --frames-in-flight 3 --no-custom-texture
```
