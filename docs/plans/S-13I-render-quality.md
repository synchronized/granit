<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-13I：跨后端渲染质量配置

## 状态

已完成。4× MSAA/Resolve、PBR Specular AA、Mipmap、各向异性过滤、FXAA、设备能力查询、
高级管线质量开关和查看器控制均已落地，三后端组合验收已通过；结果见
[2026-09-03 S-13H/S-13I 跨后端验收](../records/2026-09-03-s13hi-cross-backend-acceptance.md)。

## 背景与目标

模型查看器已经同时使用多种抗锯齿和纹理过滤技术，但调用者目前不能完整查询支持范围或选择质量
组合。S-13I 将底层设备能力与高级渲染策略分层，使应用可以明确请求质量，并得知实际生效配置。

目标包括：

- Renderer 公开查询 MSAA 样本数和 Sampler 各向异性限制。
- Render Pipeline 独立控制 MSAA、FXAA 和 Specular AA，不使用组合数量不断增长的总枚举。
- Model Viewer 提供质量控件并显示请求值、设备限制和实际生效值。
- Vulkan、桌面 Dawn 和 Emscripten WebGPU 使用相同公共配置与确定性回退规则。

## 非目标

- 不在本任务实现 TAA、SMAA、动态分辨率、Alpha-to-Coverage 或时间重建。
- 不公开 Specular AA 的算法参数，也不承诺当前导数公式为稳定 ABI。
- 不把 FXAA 或 PBR 策略下沉到 Renderer/HAL。
- 不为质量档建立另一套资源、材质或场景所有权。

## 已确认决策

- MSAA 的 Texture 样本数、Graphics Pipeline 样本数和颜色 Resolve 属于底层公共 Renderer API。
- Mipmap 层、生成命令和 Sampler 各向异性属于底层公共资源 API。
- Model Viewer 在 CPU 端生成完整 RGBA8 Mipmap 链，再通过通用上传接口提交；这样 Vulkan、
  桌面 Dawn 和浏览器 WebGPU 不依赖后端专有的纹理 Blit 能力，并对 sRGB 颜色纹理在线性空间滤波。
- FXAA 与 Specular AA 属于可选的高级 Render Pipeline 策略。
- Render Pipeline 分别保存 `sample_count`、`enable_fxaa` 和 `enable_specular_aa`，不定义
  `MSAA_4X_FXAA` 一类组合枚举。
- 首轮 MSAA 自动路径只接受 1× 和 4×；底层类型仍保留其他后端可支持的合法样本数。
- 查看器默认请求 4× MSAA、FXAA、Specular AA、完整 Mipmap 和 8× 各向异性。
- 上层只能依据公开能力做显式回退，并显示实际结果；底层接口不静默降低请求值。
- FXAA 位于 Tone Mapping 后、UI 合成前，避免模糊 ImGui 和文字。
- Specular AA 首轮只公开开关；关闭时保持材质原始感知粗糙度路径。
- Mipmap、各向异性、MSAA、Specular AA 和 FXAA 分别处理纹理缩小、斜视采样、几何边缘、
  高频镜面高光和最终图像边缘，任何一项都不替代其他项。

## 公共接口边界

`granit_renderer_limits` 在结构尾部追加样本数能力和最大各向异性，使用位掩码表达可支持的样本数，
避免把设备能力误写为单一最大值。C++20 包装只映射 C ABI，不维护第二份状态。

Render Pipeline 创建描述直接包含样本数和两个布尔策略字段，初始化宏提供当前推荐默认值。项目尚未
发布，因此创建时要求完整的当前结构，不保留旧结构大小分支。自定义录制回调维持单采样契约，
除非未来单独扩展回调描述以显式声明多采样支持。

## 实施顺序

1. 补齐 Vulkan、Dawn 和 Emscripten 的样本数及各向异性能力查询，并扩展 Renderer Limits ABI。
   （已完成）
2. 为 Render Pipeline 增加 FXAA 和 Specular AA 独立开关。（已完成）
3. 通过后端无关的每帧常量把策略传入自动 PBR 和 Tone Mapping 路径，避免为布尔组合创建 Shader
   变体，同时禁止运行时后端分支泄漏到公共层。（已完成）
4. 为 Model Viewer 增加 MSAA、FXAA、Specular AA 和各向异性控件，显示实际生效配置。
   （已完成）
5. 补充公共头、ABI、错误、回退、像素和三后端组合测试。
   （已完成；浏览器 Fixture 会在真实 Chrome WebGPU 中依次重建 1×/全关闭与 4×/全开启配置，
   按设备上限启用各向异性，并验证继续呈现及退出资源归零）
6. 完成手动 Actions 验收后更新 Reference、Guide 和带日期的验收记录。（已完成）

## 测试与验收

- 1× 与 4× MSAA 均可创建、绘制和回读；Resolve 的格式、尺寸、样本数和句柄错误被拒绝。
- 不支持 4× 的设备由查看器显式回退为 1×，底层直接请求仍返回明确错误。
- FXAA 与 Specular AA 可分别开关，四种组合均生成有效且可区分的离屏结果。
- 各向异性请求不超过公开上限；超限请求不被静默截断。
- UI 不经过 FXAA；单采样和自定义回调保持既有契约。
- Vulkan、桌面 Dawn 和浏览器 WebGPU 报告实际配置，并通过共同 Fixture 和截图容差。

## 风险与未决问题

- 样本数能力表示后端可供公共自动渲染路径使用的颜色/深度交集，不承诺任意格式组合。
- 浏览器 WebGPU 的能力仍可能不同于原生 Dawn，应用必须继续依据实际 Renderer Limits 回退。
- GitHub 托管 Windows Runner 不承担完整 IBL 图像和性能验收；真实 Windows GPU 结果不能由托管
  环境外推。
