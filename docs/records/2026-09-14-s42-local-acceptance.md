<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-14 S-42 测试架构本地验收

## 结论

S-42A～S-42F 的本地实现已完成。Smoke 从基线的 14 项收敛为两个端到端测试；Compute、PBR、
HDR 和 Render Pipeline GPU 回归已移入 `tests/gpu`；Win32 Window component 与 SDL3 外部窗口
adapter 复用同一套测试私有 Swapchain 帧逻辑。

Quick Check 现在按 CTest 标签运行 `unit` 与 `smoke`。Windows 工作流按 `gpu|platform` 标签排除
托管 Runner 无 Vulkan ICD 时不能运行的测试，Linux 与 Release Candidate 继续承担完整 GPU 成功
路径。Emscripten 的平台 Smoke、Model Viewer 和 ImGui 三个浏览器任务保持独立。

## 本地验证

验证环境为 Windows 10、Visual Studio 2022、Release 配置和本机 Vulkan 驱动。

| 配置 | 结果 |
|---|---|
| Windows VS2022 共享库 Release 构建 | 通过 |
| Windows VS2022 共享库 Release CTest | 86/86 通过 |
| Windows VS2022 静态库 Release 构建 | 通过 |
| Windows VS2022 静态库 Release CTest | 80/80 通过 |
| CTest `unit` 标签 | 21 项 |
| CTest `smoke` 标签 | 2 项 |
| CTest `gpu` 标签 | 12 项 |
| CTest `platform` 标签 | 3 项 |
| SDL3 Surface 集成测试 | 共享库配置通过 |
| 文档检查、工作流语法和 `git diff --check` | 通过 |

共享库比静态库多 6 个动态库导出检查，符合 ABI 测试条件。两个配置均执行了 GPU、窗口和两个
Smoke 的成功路径，没有以 Skip 代替本地验收。

## 待远端验收

Linux 的 XCB、Wayland、SDL3、Lavapipe，Emscripten 的三个浏览器入口，以及四套不可变 Release
Candidate SDK 需要基于已推送提交运行。远端结果通过前，S-42 保持“本地实现完成”状态，不标记为
最终完成。
