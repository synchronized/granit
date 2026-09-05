<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-06 S-26A WebGPU 能力审计

## 结论

浏览器 WebGPU 已经覆盖正式 Model Viewer 的基础渲染闭环：Canvas Surface、Swapchain、Buffer 与
Texture 上传、Sampler、Bind Group、Graphics/Compute Pipeline、动态 Uniform、索引绘制、MSAA
Resolve、深度附件、公共 PBR 和 Environment Map 均已接通。

当前差距主要位于通用资源传输、运行时 Mipmap、Timestamp Query 和用户级浏览器外壳。0.11.0
不追求 Vulkan 专属能力的表面对等，而是补齐公共 API 可表达、WebGPU 也有可靠语义的能力。

## 审计范围

- 公共 Renderer、资源、Command Recorder、Timestamp 和 Render Pipeline API。
- `backend/contracts` 中的私有 HAL 能力接口。
- Vulkan 与 WebGPU Renderer 实现以及 WebGPU Provider 操作表。
- `granit_web_platform_smoke` 和桌面 Model Viewer 的构建、资产与交互路径。

## 能力矩阵

| 公共能力 | Vulkan | 浏览器 WebGPU | 0.11.0 处理 |
|---|---|---|---|
| Renderer、Canvas、Swapchain | 支持 | 支持 | 保持并补恢复测试 |
| Buffer/Texture 创建与同步上传 | 支持 | 支持 | 保持 |
| Upload Batch | 支持 | 支持 | 正式 Viewer 分帧消费 |
| Graphics/Compute Pipeline | 支持 | 支持 | 保持 |
| 动态 Uniform、索引与实例绘制 | 支持 | 支持 | 保持 |
| 多附件、深度、MSAA Resolve | 支持 | 支持当前公共子集 | 真实模型回归 |
| Buffer 到 Buffer 复制 | 支持 | HAL 返回 `UNSUPPORTED` | S-26B 实现 |
| Buffer 到 Texture 复制 | 支持 | Provider 有早期简化入口，HAL 未接通 | S-26B 重构并实现 |
| Texture 到 Buffer 复制/回读 | 支持 | 单区域路径已接通 | S-26B 扩展回归 |
| Texture 到 Texture 复制 | 支持 | HAL 返回 `UNSUPPORTED` | S-26B 实现 |
| Buffer 填充 | 支持 | HAL 返回 `UNSUPPORTED` | S-26B 实现 |
| Mipmap 生成 | 线性 Blit | 未实现 | S-26C 使用 WebGPU Pass |
| Timestamp Query | 支持 | 未暴露 HAL 接口 | S-26D 按 Feature 可选实现 |
| 资源数组 | 支持公共描述 | WebGPU 限制 `array_count = 1` | 无真实需求，暂缓 |
| Compare Sampler | Vulkan 可表达 | 公共 Layout 缺少绑定子类型 | 不在本版本强行加入 |
| LOD Bias | 支持 | WebGPU 无对应字段 | 固有限制，保持拒绝 |
| 持久映射 | 支持适用内存 | 浏览器不允许同等语义 | 固有限制，使用上传/回读 |
| Pipeline Cache | 支持 | 浏览器无可靠同等契约 | 固有限制，不模拟 |
| 原生同步与 Barrier | 内部支持 | 浏览器隐式跟踪 | 不进入公共 API |

## Model Viewer 差距

现有 `granit_web_platform_smoke` 已复用 Model Viewer Core，但它仍是自动化测试目标：加载小型 Fixture，
没有完整 ImGui 外壳，也没有把真实 Flight Helmet、正式环境资产、加载进度和错误恢复作为用户路径。

0.11.0 保留该 Smoke，并新增 `granit_model_viewer_web`。正式目标复用桌面端 CPU Scene、GPU Scene、
公共 PBR 和 Environment Map，只在平台层实现 HTTP Fetch、Canvas/DPI、浏览器输入和同步帧执行。

## 实施边界

- S-26B 可直接扩展现有私有 Provider 操作表；该操作表只在 Emscripten 静态目标内部使用，不形成
  新公共 ABI。
- S-26C 复用现有公共 `generate_mipmaps`，后端选择不同算法，不新增 WebGPU 专用入口。
- S-26D 复用现有 Timestamp 公共 API；只有能力快照需要准确反映可选 Feature。
- 正式 Viewer 的 Fetch、资源 URL、进度和页面状态保持示例私有，不进入 Granit 公共组件。

## 验收依据

- 所有新增能力必须先通过后端无关 Registry 测试，再通过真实 Emscripten WebGPU 浏览器测试。
- 不支持项必须由能力查询或稳定结果码提前表达，不能静默跳过命令或降低资源质量。
- Model Viewer 验收使用同一正式模型、环境和 Shader 资产比较 Vulkan 与 WebGPU 输出。
