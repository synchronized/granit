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

## 覆盖与限制

Windows 构建覆盖 Win32、SDL3、Triangle、HDR 和 ImGui 示例。XCB 与 Wayland 源码已迁移，但本地
Windows 环境无法编译运行；Linux、安装 Consumer 和跨平台 Actions 留到特性分支完成后的统一 CI
验证。
