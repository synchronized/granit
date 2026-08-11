<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-03：金属度/粗糙度 PBR 渲染模块

## 元数据

- 设计状态：已确认首版边界
- 实现状态：进行中（H-03A）
- 路线图任务：H-03
- 优先级：P2
- 前置依赖：H-01、H-02
- 后续依赖：H-04、H-05

## 目标

H-03 在 `granit::material` 和核心 Renderer 之上提供可选的金属度/粗糙度 PBR 参考模块，验证材质
包、参数绑定、顶点输入、深度测试和实际 Draw 能组成稳定闭环。首版以离屏、前向、不透明、单个
方向光为验收场景，不接管 Scene、Camera、Light 集合、资产数据库或窗口生命周期。

该模块是可替换参考实现，不是核心 Renderer 的固定渲染模型。使用者仍可直接使用显式 Pipeline、
Bind Group 和 Render Graph API，或构建完全不同的材质工作流。

## 首版渲染边界

### 包含

- Vulkan 风格右手坐标约定下的静态三角网格。
- 单个颜色 Attachment 和可选深度 Attachment。
- 不透明、单面或双面三角形的前向渲染。
- 金属度/粗糙度工作流及直接光照的 Cook-Torrance BRDF。
- Base Color、Metallic/Roughness、Normal、Occlusion 和 Emissive 参数/纹理槽约定。
- CPU 参考 BRDF、固定输入图像或像素结果测试，以及离屏端到端示例。
- 显式输入的 View、Object 和单方向光数据；H-03 不保存或遍历场景对象。

### 不包含

- 阴影、IBL、反射探针、环境预过滤、BRDF LUT 和天空盒，这些进入 H-05。
- 点光、聚光、光源列表、聚簇/分块光照和可见性筛选，这些进入 H-04/H-05。
- 透明、折射、清漆、次表面、各向异性、Sheen 或 Specular 扩展。
- 蒙皮、Morph、实例化、Mesh Shader、Ray Tracing 或 Bindless 路径。
- glTF 导入、图片解码、Mip 生成、纹理压缩和资产热重载。
- 自动曝光、Bloom、抗锯齿和 Tone Mapping；首版输出线性 HDR 或测试指定格式。

## 数据与绑定约定

沿用 H-02 的分组频率：

| Group | H-03 首版内容 | 所有者 |
| ---: | --- | --- |
| 0 | View/Projection、相机位置、单方向光方向与辐照度 | 调用方输入，H-03 上传 |
| 1 | PBR 材质常量和 Texture/Sampler | `granit::material` |
| 2 | Model、Normal Matrix 和对象标识 | 调用方输入，H-03 上传 |
| 3 | 首版为空，保留给阴影或 Pass 数据 | H-05 |

首版标准顶点语义为：位置 `float3`、法线 `float3`、切线 `float4`、第一组 UV `float2`。最小无纹理
路径只要求位置和法线；启用任意纹理需要 UV，启用法线贴图还需要带 handedness 的切线。缺失必需
语义必须在创建 Pipeline 或提交 Draw 前明确失败，不能静默填零。

材质常量至少包含线性 Base Color、Metallic、Perceptual Roughness、Normal Scale、Occlusion
Strength 和线性 Emissive。颜色纹理按 sRGB 采样，法线、Metallic/Roughness 和 Occlusion 纹理按
线性数据采样。首版不自动根据文件名或图片格式猜测颜色空间。

## BRDF 约定

- 漫反射使用 Lambert，并按金属度减弱介电漫反射。
- 法线分布使用 GGX/Trowbridge-Reitz。
- 几何遮蔽使用 Smith correlated GGX。
- Fresnel 使用 Schlick 近似，介电基础反射率首版固定为 `F0 = 0.04`。
- Roughness 在进入 BRDF 前限制到非零下限，防止除零和不稳定高光。
- 所有光照计算在线性空间进行；首版 Shader 不负责最终显示 Tone Mapping。

CPU 参考函数与 Shader 必须共享公式说明和固定测试向量。允许实现为了 GPU 数值稳定做等价变形，
但不能在 CPU 与 Shader 路径使用不同的材质语义。

## 必须先补齐的 H-02 契约

当前 `material_package` 的 Graphics Pipeline 仍依赖默认图元状态，且没有持久化顶点布局。H-03
不得在模块内部再建立一套隐藏 Pipeline 描述，必须先扩展现有包：

- Variant 或 Pass 显式保存顶点 Buffer/Attribute 布局。
- Pass 显式保存 topology、front face、cull mode、polygon mode、深度测试/写入和颜色混合状态。
- 上述字段进入 `.grmat` 规范化编码、调试 JSON、源 JSON 和完整 Pipeline 缓存键。
- 旧格式仍处于开发阶段，可以直接提升格式版本，不承诺自动迁移。

这部分记为 H-03A，而不是重新打开 H-02；它由第一个真实高层消费者驱动，并继续复用 H-02 的包与
缓存抽象。

## 分阶段实施

1. **H-03A（进行中）**：扩展材质包的顶点布局和固定 Pipeline 状态，覆盖源 JSON、二进制往返、
   调试导出、缓存键及损坏输入测试。
2. **H-03B**：实现独立 CPU BRDF 参考函数、PBR 参数规范、边界值和固定测试向量。
3. **H-03C**：加入无纹理 HLSL PBR Shader、离线构建包和单三角/球体离屏直接光照闭环。
4. **H-03D**：接入 Base Color、Metallic/Roughness、Normal、Occlusion、Emissive 纹理与变体，
   验证 sRGB/线性语义和缺失顶点属性诊断。
5. **H-03E**：封装显式 View/Object/Directional Light 输入和 Render Graph Pass 适配器，不创建
   Scene 或 Light 管理器。
6. **H-03F**：增加端到端图像/数值回归、资源生命周期测试和性能基线，整理 H-04/H-05 输入契约。

## 验收标准

- PBR 模块不包含 Vulkan 类型，核心 `granit` 不反向依赖 PBR。
- 无纹理材质能在离屏颜色和深度目标上完成真实 Draw。
- CPU 参考 BRDF 与 Shader 固定测试结果在明确误差范围内一致。
- 参数和纹理颜色空间、默认值、范围及所有权均有明确文档。
- 缺失变体、顶点属性、资源或 Pipeline 创建失败时返回明确诊断，并可使用 H-02 错误材质。
- H-03 不创建 Scene、Camera 或 Light 对象；调用方可以逐 Draw 显式提供输入。
- 阴影、IBL 和后处理不以临时隐藏实现混入 H-03。
