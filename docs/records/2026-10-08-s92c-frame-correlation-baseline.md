<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92C Frame Context 关联基线

## 实现

- Command Recorder 内部记录当前 `active_frame`，只在 Frame Context 录制期间有效；
- Frame Context begin、submit、abort 和 swapchain present 写入 `frame` 事件；
- Timestamp create/read 等无录制上下文的操作使用 `frame_id=0`，reset/write 从当前 recorder 读取实际
  `frame_id`；
- 事件只写入公共句柄数值和结果码，不写入 Vulkan、WebGPU 或平台对象；
- Frame Context 结束或中止后清除关联，避免复用 recorder 时串帧。

## 验证

- `cmake --build build/windows-clang-debug --target granit_renderer_test -j 4`：通过；
- `ctest` 核心生命周期与 Renderer 资源测试：通过；
- Renderer 运行时 Trace：37 条记录逐行通过 `ConvertFrom-Json`，其中 12 条 `frame`、4 条 `timestamp`、
  7 条 `resource_stats`。

## 未完成项

Pipeline/Descriptor/Barrier 统计、资源创建销毁事件、Fence/retirement 关联，以及 WebGPU 错误/设备丢失和
Shader 编译事件的 frame 关联仍待后续 S-92C/S-92D 实现。
