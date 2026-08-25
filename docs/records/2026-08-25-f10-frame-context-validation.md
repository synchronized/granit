<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-25 F-10 Frame Context 验证记录

## 结论

公共 Frame Context、Canvas 动态帧槽和实时窗口示例迁移已在 Windows Clang Debug 环境通过构建、
完整测试及 SDL3 + ImGui 连续 2,000 帧验证。运行期间未报告 Validation Layer、帧生命周期或资源
泄漏错误。

## 环境与命令

- 系统：Windows 10，64 位。
- 编译器：Clang 22.1.8。
- 构建 preset：`windows-clang-debug` 与 `windows-clang-static-debug`。
- 基线提交：`03415a1`，其后仅增加本次测量入口与文档。

```powershell
cmake --build --preset windows-clang-debug
ctest --preset windows-clang-debug --output-on-failure
.\build\windows-clang-debug\bin\granit_sdl3_imgui_example.exe --frame-count 2000
```

## 结果

- 共享库完整测试：47/47 通过。
- 静态库完整测试：43/43 通过。
- 连续运行：2,000 帧完成，进程正常退出。
- CPU 帧时间：1.975 ms。
- GPU 时间：0.446 ms。
- Present 调用时间：0.041 ms。
- Frame Context 槽位等待：0.158 ms。

时间为示例指数平滑后的退出时数值，用于确认等待没有重新退化为逐帧串行，不代表跨设备性能
基准。吞吐、输入延迟和不同 `frames_in_flight` 档位仍应在发布性能结论前使用 Release 构建重复测量。

## Release 性能拆分

示例增加 `--frames-in-flight`、`--no-validation`、`--no-demo` 和 `--no-gpu-timestamps` 参数，并
分别报告 ImGui 构建、Draw Data 转换与渲染帧循环时间。Windows Clang Release 每组运行 2,000 帧，
退出时指数平滑值如下：

| 场景 | CPU | ImGui | 转换 | 渲染 | 约合 FPS |
|---|---:|---:|---:|---:|---:|
| 3 槽完整功能 | 1.112 ms | 0.104 ms | 0.069 ms | 0.907 ms | 899 |
| 3 槽关闭 Validation | 0.896 ms | 0.181 ms | 0.099 ms | 0.603 ms | 1,116 |
| 3 槽关闭 Demo | 0.763 ms | 0.080 ms | 0.031 ms | 0.643 ms | 1,310 |
| 3 槽关闭 GPU 时间戳 | 1.007 ms | 0.125 ms | 0.058 ms | 0.813 ms | 993 |

关闭 Validation、Demo 和 GPU 时间戳后的槽位对照：

| 在途帧槽 | CPU | 渲染 | 槽等待 | 约合 FPS |
|---:|---:|---:|---:|---:|
| 1 | 1.264 ms | 1.114 ms | 0.078 ms | 791 |
| 2 | 1.330 ms | 1.182 ms | 0.051 ms | 752 |
| 3 | 0.988 ms | 0.747 ms | 0.049 ms | 1,012 |
| 4 | 0.711 ms | 0.606 ms | 0.032 ms | 1,406 |

单次短测存在调度波动，但结论一致：ImGui 转换不是主瓶颈，主要时间位于 acquire、Canvas 录制、
提交和呈现组成的渲染阶段。增加槽数能提高吞吐，但可能增加输入到显示延迟，因此默认值仍不应仅按
最高 FPS 选择。后续优化应继续拆分 Canvas 上传/flush、绑定更新和 Queue 提交成本。

进一步将渲染阶段拆分后，3,000 帧 Release 测量如下：

| 场景 | Acquire | Canvas Record | Submit | Present | CPU |
|---|---:|---:|---:|---:|---:|
| 3 槽完整功能 | 0.094 ms | 0.353 ms | 0.174 ms | 0.037 ms | 0.983 ms |
| 3 槽完全精简 | 0.400 ms | 0.104 ms | 0.055 ms | 0.015 ms | 0.734 ms |
| 4 槽完全精简 | 0.357 ms | 0.107 ms | 0.053 ms | 0.013 ms | 0.687 ms |

独立 Canvas CPU 基准中，100 个矩形的持久映射几何上传与 flush 约为 0.001 ms，1,000 个矩形约
0.004 ms，因此 ImGui 场景的上传不是主要瓶颈。GPU 基准中 100 个兼容 Item 合并为一个 Batch 时
约 0.033 ms；100 个交替纹理 Batch 约 0.811 ms，说明频繁纹理切换导致的结束 Rendering、材质更新
和重新开始 Rendering 代价显著。

完全精简场景当前最大单项是 Swapchain acquire 等待，其次是 Canvas Record；Queue submit 和
Present 不是首要瓶颈。Validation 会明显放大 Canvas 绑定校验与提交成本。后续代码优化应优先复用
Canvas 每帧绑定，并为多纹理场景避免每次纹理变化都切断 Dynamic Rendering；acquire 等待应作为
驱动帧节流和输入延迟指标单独观察。

## 覆盖与限制

Windows 构建覆盖 Win32、SDL3、Triangle、HDR 和 ImGui 示例。XCB 与 Wayland 源码已迁移，但本地
Windows 环境无法编译运行；Linux、安装 Consumer 和跨平台 Actions 留到特性分支完成后的统一 CI
验证。
