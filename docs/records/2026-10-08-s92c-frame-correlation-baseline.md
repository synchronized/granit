<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92C Frame Context 关联基线

## 实现

- Command Recorder 内部记录当前 `active_frame`，只在 Frame Context 录制期间有效；
- Frame Context begin、submit、abort 和 swapchain present 写入 `frame` 事件；
- Timestamp create/read 等无录制上下文的操作使用 `frame_id=0`，reset/write 从当前 recorder 读取实际
  `frame_id`；
- Buffer、Texture、TextureView 和 Timestamp Query Pool 的创建/销毁写入 `resource` 事件，包含句柄、内部
  creation sequence、大小或数量和结果码；资源回收仍由原有 retirement 队列负责；
- Pipeline 绑定、Bind Group（Descriptor）绑定、Draw 和 Dispatch 写入 `command` 事件，包含 recorder、
  frame id、数量和结果码；
- WebGPU Pipeline/Shader 异步诊断进入统一 `diagnostic` 事件；无法安全归属异步回调的事件明确使用
  `frame_id=0`，不伪造当前帧；
- Timestamp 后端返回 `UNSUPPORTED` 时写入 `availability=unavailable`，用于明确区分“设备没有该能力”和
  “查询尚未完成”；
- 事件只写入公共句柄数值和结果码，不写入 Vulkan、WebGPU 或平台对象；
- Frame Context 结束或中止后清除关联，避免复用 recorder 时串帧。

## 验证

- `cmake --build build/windows-clang-debug --target granit_renderer_test -j 4`：通过；
- `ctest` 核心生命周期与 Renderer 资源测试：通过；
- Renderer 运行时 Trace：302 条记录逐行通过 `ConvertFrom-Json`，其中 12 条 `frame`、4 条 `timestamp`、
  265 条 `resource`、7 条 `resource_stats` 和 14 条 `diagnostic`。

## 未完成项

Barrier 统计、Fence/retirement 事件的独立关联，以及 WebGPU Shader 编译事件和浏览器 E2E 运行时验证仍待
后续 S-92C/S-92D 实现。
