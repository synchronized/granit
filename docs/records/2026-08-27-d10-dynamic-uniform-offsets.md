<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-27 D-10 动态 Uniform Buffer Offset 验收

## 结论

D-10 已完成。C/C++ 公共契约、Vulkan 动态 Descriptor、Graphics/Compute 命令绑定、设备限制
查询、验证规则、双对象像素 Smoke Test 和跨平台安装矩阵均通过。当前行为以
[Pipeline 参考](../reference/pipeline.md)和
[Command Recorder 参考](../reference/command-recorder.md)以及
[Renderer 参考](../reference/renderer.md)为准。

## 功能验收

- 动态 Offset 按 Bind Group 参数顺序和组内 `binding` 升序消费，普通 Binding 不占用 Offset。
- 数量、设备对齐、基础范围、动态范围和整数溢出均有失败路径覆盖。
- 同一 Uniform Buffer 的 `0` 和 `256` Offset 分别绘制红、绿两个对象，像素回读符合预期。
- 双对象测试启用 Vulkan Validation Layer，Descriptor、Offset、Range 和生命周期错误计数为零。
- `granit_renderer_get_limits` 与 C++ 包装可查询设备实际 Uniform Buffer 动态偏移对齐和最大绑定
  范围；无效参数、结构尺寸、失效句柄和安装 Consumer 均有覆盖。

## 构建与安装验收

- Windows Clang Shared Debug：构建成功，58/58 测试通过。
- Windows Clang Static Release：构建成功，54/54 测试、安装导出审计及 7/7 独立 Consumer 通过。
- [Windows Actions](https://github.com/synchronized/granit/actions/runs/33062792883)：MSVC Shared/Static、
  安装导出及 C/C++ Consumer 全部通过。
- [Linux Actions](https://github.com/synchronized/granit/actions/runs/33062796709)：Clang/GCC ×
  Shared/Static、安装 Consumer，以及 SDL3/ImGui X11/Wayland Integration Runtime 全部通过。
- [D-10F Windows Actions](https://github.com/synchronized/granit/actions/runs/33065960998)：MSVC
  Shared/Static、安装导出及 C/C++ Consumer 全部通过。
- [D-10F Linux Actions](https://github.com/synchronized/granit/actions/runs/33066417916)：Clang/GCC ×
  Shared/Static、精确 ABI 导出快照、安装 Consumer 及 Integration Runtime 全部通过。

验收期间发现安装包的 `newer_minor` 检查仍请求当前 `0.4`，已改为请求未来 `0.5`，恢复版本拒绝
测试的有效性。D-10F 首轮 Linux 共享矩阵还发现新增公共符号未登记精确导出快照；同步快照后
完整矩阵通过。
