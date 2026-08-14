<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-03：金属度/粗糙度 PBR 渲染模块

## 状态

- 路线图任务：H-03
- 优先级：P2
- 状态：已完成
- 前置依赖：H-01 Render Graph、H-02 Material
- 后续依赖：H-04 Scene、H-05 Lighting、H-07 Render Pipeline
- 历史记录：[H-03 实施记录](../records/H-03-pbr-renderer-implementation.md)

## 目标与定位

H-03 在 Material 和核心 Renderer 之上提供可选的金属度/粗糙度 PBR 参考模块，验证材质包、参数
绑定、顶点输入、深度测试和真实 Draw 能组成稳定闭环。

该模块是可替换的 Forward PBR 参考实现，不是核心 Renderer 的固定渲染模型，也不接管 Scene、
Camera、Light 集合、资产数据库或窗口生命周期。

## 渲染边界

首版包含：

- 静态三角网格、颜色与可选深度 Attachment。
- 不透明、单面或双面 Forward Rendering。
- 金属度/粗糙度工作流与 Cook-Torrance 直接光照。
- Base Color、Metallic/Roughness、Normal、Occlusion 和 Emissive 参数及纹理。
- 显式 View、Object 和单方向光输入。
- CPU BRDF、离屏像素回归、生命周期和性能测试。

首版不包含：

- 阴影、IBL、反射探针和 Tone Mapping；这些由 H-05 提供。
- 多光源可见性、Scene 管理和资产导入。
- 透明、折射、清漆、次表面、各向异性或 Sheen。
- 蒙皮、Morph、Mesh Shader、Ray Tracing 或强制 Bindless 路径。

## 数据与绑定

| Group | 内容 | 所有者 |
|---:|---|---|
| 0 | View/Projection、相机和基础帧数据 | 调用方输入，PBR 上传 |
| 1 | PBR 材质常量和纹理/Sampler | Material |
| 2 | Model、Normal Matrix 和对象标识 | 调用方输入，PBR 上传 |
| 3 | 阴影、IBL 与光源 | H-05 Lighting |

标准顶点语义为位置 `float3`、法线 `float3`、切线 `float4` 和第一组 UV `float2`。无纹理路径至少
要求位置与法线；纹理路径需要 UV，法线贴图额外需要带 handedness 的切线。缺失必需语义必须在
创建 Pipeline 或提交 Draw 前明确失败。

颜色输入使用线性 Base Color 和 Emissive；颜色纹理按 sRGB 采样，法线、Metallic/Roughness 和
Occlusion 按线性数据采样。系统不根据文件名猜测颜色空间。

## BRDF 约定

- 漫反射使用 Lambert，并按金属度减弱介电漫反射。
- 法线分布使用 GGX/Trowbridge-Reitz。
- 几何遮蔽使用 Smith correlated GGX。
- Fresnel 使用 Schlick 近似，介电基础反射率为 `F0 = 0.04`。
- Perceptual Roughness 限制到非零下限，避免不稳定高光。
- 所有光照在线性空间完成，PBR Shader 不负责最终 Tone Mapping。

CPU 参考实现和 Shader 共享同一公式与固定测试向量；允许数值稳定所需的等价变形，但不得产生两套
材质语义。

## 完成结果

H-03 已完成：

- 完整 Graphics Pipeline 状态进入材质包、JSON、二进制格式和缓存键。
- CPU BRDF 参考实现、边界测试和固定向量。
- HLSL PBR Shader、预编译 SPIR-V 和离屏直接光照闭环。
- PBR 常量、五类纹理、默认资源和编译期纹理变体裁剪。
- 显式 View/Object/Directional Light GPU 布局和 Draw 输入。
- Render Graph Pass 适配、生命周期测试、CPU 基线和 GPU 像素回归。

逐阶段过程见[H-03 实施记录](../records/H-03-pbr-renderer-implementation.md)。

## 验收标准

- PBR 模块不包含 Vulkan 类型，核心 Renderer 不反向依赖 PBR。
- 无纹理和纹理材质能在离屏颜色与深度目标完成真实 Draw。
- CPU BRDF 与 Shader 像素结果在明确误差范围内一致。
- 参数、纹理颜色空间、默认值、范围和所有权均有确定语义。
- 缺失变体、顶点属性、资源或 Pipeline 时返回明确诊断。
- PBR 不创建 Scene、Camera 或 Light 管理对象，也不隐藏混入 H-05 功能。
