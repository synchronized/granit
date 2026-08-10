<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-05：基础绘制命令

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：D-05
- 优先级：P0
- 前置依赖：D-03、D-04、F-05
- 后续依赖：D-06

## 已实现能力

- 批量设置动态 Viewport 和 Scissor。
- 批量绑定 Vertex Buffer，绑定 UINT16 或 UINT32 Index Buffer。
- 录制非索引 Draw 和 Draw Indexed，支持实例数量和基础偏移参数。
- Vertex/Index Buffer 自动进入 Vertex Input 读取状态，并由 Recorder 保持到提交完成。
- Draw 前校验录制状态、Dynamic Rendering、Graphics Pipeline、Viewport、Scissor 和 Index Buffer。
- Clear 继续使用统一 Attachment 的 Load Operation，不增加独立且含义重叠的清除命令。

## 第一版边界

Vertex/Index Buffer 必须在 `begin_rendering` 前绑定，使自动 Buffer 屏障录制在渲染区域之外。
Pipeline、Bind Group、Viewport 和 Scissor 可以在渲染区域内外设置。第一版 Graphics Pipeline 尚未
开放 Vertex Attribute Layout，因此最小三角形优先使用 `vertex_index` 生成顶点；Buffer 绑定和
Draw Indexed 已打通生命周期与命令路径，Vertex Layout 随后按实际格式需求扩展。

## 验收

- Validation Layer 下完成离屏 Dynamic Rendering、Draw 和 Draw Indexed。
- 错误状态和零 Draw 数量返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- 录制后销毁公开 Pipeline 和 Buffer 句柄仍可安全提交。
- C11/C++20 头文件、共享库和静态库构建保持通过。
