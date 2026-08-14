<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-06 Unlit、2D 与 UI 实施记录

## 结果

H-06A～H-06E 已完成内部技术路线验证：共享 Unlit 着色、透明与裁剪、UI Draw List、动态上传、
批处理、参考管线 UI 扩展点以及调试绘制/文字边界均已闭合。公共 UI、Debug Draw 和 Text ABI
仍需分别设计，内部类型不构成兼容承诺。

## Unlit 与透明

- 建立共享 Unlit HLSL、`unlit_opaque`、`unlit_alpha_cutoff` 和 `unlit_transparent` Pass。
- 基础颜色由材质颜色、可选纹理和可选顶点色相乘；不读取 PBR 光照、阴影或 IBL。
- 透明路径使用预乘 Alpha，支持显式 Load/Clear、稳定顺序和 Scissor。
- SPIR-V 反射、材质包构建和离屏像素回归覆盖 Opaque、Cutoff、透明叠加及裁剪边界。

## UI Draw List 与批处理

- 内部 `ui_vertex` 使用位置、UV 和 RGBA8 UNORM 顶点色；局部索引在追加时转换为全局索引。
- 仅合并相邻且 Texture、Sampler、Scissor 和层级一致的 Item，不跨透明项重排。
- 动态上传对象复用可增长的 Upload Vertex/Index Buffer，移动和析构负责句柄生命周期。
- `unlit_ui` 材质包和 UI Pass 支持纹理、Sampler、顶点色、Scissor 与 Indexed Draw。
- 测试覆盖索引修正、顺序、合批、资源切换、空列表、无效输入、复用和像素结果。

## 测量结果

- 修复索引容器 O(N²) 增长后，10,000 矩形 CPU 构建 P50 为 1.303～1.386 ms；详见
  [UI CPU 基线](../../benchmarks/results/2026-08-14-windows-clang-ui-056a0c8.md)。
- 10,000 矩形单 Batch GPU P50 为 1.864～1.897 ms，逐项交替状态为 81.309～82.837 ms；详见
  [UI GPU 基线](../../benchmarks/results/2026-08-14-windows-clang-ui-gpu-f77b112.md)。
- 结果支持优先保持相邻兼容性，尚不足以证明需要立即引入 Bindless。

## 参考管线集成

- `GRANIT_RENDER_PIPELINE_STAGE_UI` 在 Tone Mapping 后操作同一显示空间输出，并要求 Attachment
  `LOAD` 保留场景颜色。
- 阶段不携带 Scene Draw、深度、阴影或 IBL，且不再次应用曝光或 Tone Mapping。
- `encode_srgb` 区分 UNORM Shader 编码与 sRGB Attachment 编码；测试覆盖两种格式和多 View。
- 离屏与 Swapchain Frame 共用 Render Graph 构建路径，尺寸和 Viewport 每次渲染更新。

## H-06E 边界评估

- Debug Draw 生成临时 Unlit 或 UI 几何，不在 Renderer 保存调试命令状态。
- Text component 负责整形、栅格化和字形 Atlas，只向 UI Draw List 输出已定位四边形。
- 第三方 UI 使用适配层，不成为核心传递依赖。
- 完整决策见 [ADR-001](../decisions/ADR-001-debug-text-boundary.md)。
