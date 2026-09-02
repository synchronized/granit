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
- 首个环境源锁定 Poly Haven `Studio Small 03` 1K HDR，作者 Greg Zaal，许可证 CC0-1.0；
  manifest 固定来源元数据、字节数和 SHA-256，原始文件不进入默认工作树。
- glTF IBL Sampler 仅作为 Apache-2.0 离线生成工具；运行时不链接它，也不增加 KTX 依赖。
- 离线转换器只接受 IBL Sampler 输出的未压缩 `R16G16B16A16_SFLOAT` KTX2 Cube；受限解析器
  拒绝 Supercompression、非六面纹理、数组纹理、错误 Mip 尺寸、重叠与越界 Level。
- Base Color、Normals、Metallic 和 Roughness 模式保持不受环境光影响，作为分层诊断入口。
- model-viewer 默认创建无需下载资产的低分辨率摄影棚环境，保证离线首次运行可读；锁定的
  `Studio Small 03` GRENV 仍作为高质量参考环境和跨后端验收输入。
- 查看器默认主光从相机侧照向模型正面，外部 HDR 环境强度默认为 `0.15`。较低的环境默认值用于
  保留 FlightHelmet 的 Base Color 和材质层次，用户仍可在 Lighting 面板中提高强度。

## 运行时环境包

example 私有 `GRENV` v1 使用小端序固定头和紧密排列的 RGBA16F 载荷。载荷依次为一个六面
Irradiance Cube、从最大到 1×1 的六面 GGX Mip 链和二维 BRDF LUT。解析器严格校验魔数、版本、
保留字段、2 的幂尺寸、合法 Mip 数、整数溢出及精确文件长度；GPU 上传层只消费解析结果并拥有
Texture 与 Texture View。该格式不是 Granit 公共资产格式，不进入安装接口兼容承诺。

## 实施顺序

1. 为 Render Pipeline 增加后端无关的可选环境纹理与采样参数输入。（已完成）
2. 锁定 CC0 摄影棚 HDRI 和来源校验清单。（已完成）
3. 定义并验证运行时环境包解析及 GPU 上传。（已完成）
4. 增加受限 KTX2 Cube 输入解析。（已完成）
5. 增加 KTX2、BRDF LUT 到确定性 GRENV 的打包工具。（已完成）
6. 生成漫反射 Cube、带 Mip 的 GGX 预过滤 Cube 与 BRDF LUT，并加入可复现校验；桌面入口支持
   通过 `--environment` 加载生成的 GRENV。（已完成）
7. 扩展 model-viewer Shader 的 Group 3 声明和 split-sum IBL 计算，移除临时摄影棚常量补光。
   （已完成）
8. 增加环境强度与旋转控件，并同步桌面及浏览器入口。（已完成）
9. 更新 Vulkan 参考图，验证桌面 Dawn 和 Emscripten WebGPU 的颜色、轮廓与深度容差。
   桌面 Dawn 验收同时保留 Vulkan 与 WebGPU 的 Base Color、最终法线、几何法线、采样法线、
   原始顶点法线/切线、Metallic 和 Roughness 分层图，用于区分资产绑定、顶点布局、几何变换、
   TBN 和最终光照差异。
   （进行中）

## 测试与验收

- Base Color 模式与现有结果一致；Shaded 模式保留木纹、镜片和零件颜色。
- 金属区域在关闭方向光后仍能看到环境反射，粗糙度会改变反射清晰度。
- Vulkan 与 WebGPU 使用同一预处理资源，Shader 绑定不包含后端条件分支。
- 资产许可证、哈希、生成命令和截图基线均可审查、可复现。

## 风险与未决问题

- 当前 Texture API 对 Cube Mip 上传和 RGBA16F 已具备基础能力，但 Emscripten 实机仍需验证。
- 若环境资源体积过大，优先降低 Cube 分辨率，不在普通构建中在线下载。
