<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-13H：模型查看器环境光照

## 状态

实施中。

## 背景与目标

FlightHelmet 的 Base Color、法线、金属度和粗糙度纹理已经正确上传与采样，但 Shaded 模式缺少
真实环境反射。目标是在不扩展公共场景所有权的前提下，让同一个模型查看器在 Vulkan、桌面 Dawn
和 Emscripten WebGPU 上使用一致的图像式光照。

## 非目标

- 不把 HDRI 文件格式、资产缓存或场景环境所有权加入 Granit 公共 API。
- 不在运行时执行高成本环境卷积。
- 不以继续增加常量环境色代替 IBL。

## 已确认决策

- 复用 Render Pipeline 的 Group 3：常量位于 binding 3，漫反射 Cube、预过滤镜面 Cube、BRDF
  LUT 和 Sampler 位于 binding 4～7。
- model-viewer PBR Shader 直接消费该后端无关布局，不出现 Vulkan 或 WebGPU 类型。
- 环境资源离线生成并锁定；源 HDRI 必须允许再分发，记录来源、版本、许可证与校验值。
- 首个环境使用中性摄影棚光，UI 后续只控制旋转和强度；方向光继续作为可调主光。
- Base Color、Normals、Metallic 和 Roughness 模式保持不受环境光影响，作为分层诊断入口。

## 实施顺序

1. 锁定 CC0 摄影棚 HDRI 和离线预处理参数。
2. 生成漫反射 Cube、带 Mip 的 GGX 预过滤 Cube 与 BRDF LUT，并加入可复现校验。
3. 扩展 model-viewer Shader 的 Group 3 声明和 split-sum IBL 计算，移除临时摄影棚常量补光。
4. 增加环境强度与旋转控件，并同步桌面及浏览器入口。
5. 更新 Vulkan 参考图，验证桌面 Dawn 和 Emscripten WebGPU 的颜色、轮廓与深度容差。

## 测试与验收

- Base Color 模式与现有结果一致；Shaded 模式保留木纹、镜片和零件颜色。
- 金属区域在关闭方向光后仍能看到环境反射，粗糙度会改变反射清晰度。
- Vulkan 与 WebGPU 使用同一预处理资源，Shader 绑定不包含后端条件分支。
- 资产许可证、哈希、生成命令和截图基线均可审查、可复现。

## 风险与未决问题

- 当前 Texture API 对 Cube Mip 上传和 RGBA16F 已具备基础能力，但 Emscripten 实机仍需验证。
- 若环境资源体积过大，优先降低 Cube 分辨率，不在普通构建中在线下载。
