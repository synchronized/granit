<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-92B WebGPU 调试标记

## 实现

- WebGPU Command Encoder、Render Pass 和 Compute Pass 设置稳定 label；
- Render Pass / Compute Pass 录制期间写入对应 Debug Group，并在结束或销毁前平衡 pop；
- WebGPU 统一 `emit` 出口接入内部 Frame Trace，覆盖初始化诊断、uncaptured error 和 device lost；
- 只使用 WebGPU 公共 C API，不调用 Dawn 私有接口；浏览器或实现忽略标记时不影响渲染；
- 事件名称与 Vulkan 侧 `granit.command_recorder`、`granit.rendering`、`granit.compute` 保持一致。

## 验证

- `cmake --build build/emscripten-debug --target granit -j 4`：通过；
- Emscripten 编译启用 `-Werror`，label/debug group API 由实际 emdawnwebgpu 端口验证；
- Windows 主库与现有 Renderer 测试保持通过。

## 未完成项

WebGPU `timestamp-query` 能力缺失的显式 `unavailable` 事件、Shader 编译诊断和浏览器 E2E 运行时
验证仍待 S-92C/S-92D；这些验证不引入浏览器工具或 Dawn 私有依赖。
