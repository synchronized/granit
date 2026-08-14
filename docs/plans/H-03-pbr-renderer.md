<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-03：金属度/粗糙度 PBR 渲染模块

## 元数据

- 设计状态：已确认首版边界
- 实现状态：已完成（H-03A～H-03F）
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

1. **H-03A（已完成）**：Pipeline 状态已进入内存模型、真实创建、二进制 v2、源 JSON 和可读
   调试 JSON，并覆盖往返、无效枚举与损坏输入检查。
2. **H-03B（已完成）**：实现独立 CPU BRDF 参考函数、PBR 参数规范、边界值和固定测试向量。
3. **H-03C（已完成）**：加入无纹理 HLSL PBR Shader、离线构建包和单三角离屏直接光照闭环。
4. **H-03D（已完成）**：材质常量、五类纹理、默认资源、顶点契约与按纹理掩码编译期裁剪已经
   贯通，并验证 sRGB/线性格式语义。
5. **H-03E（已完成）**：已封装显式 View/Object/Directional Light 输入，并建立独立 Render
   Graph Pass 适配目标，不创建 Scene 或 Light 管理器。
6. **H-03F（已完成）**：已完成数值与 GPU 像素回归、适配器生命周期测试、CPU 性能基线以及
   H-04/H-05 输入契约。

## 验收标准

- PBR 模块不包含 Vulkan 类型，核心 `granit` 不反向依赖 PBR。
- 无纹理材质能在离屏颜色和深度目标上完成真实 Draw。
- CPU 参考 BRDF 与 Shader 固定测试结果在明确误差范围内一致。
- 参数和纹理颜色空间、默认值、范围及所有权均有明确文档。
- 缺失变体、顶点属性、资源或 Pipeline 创建失败时返回明确诊断，并可使用 H-02 错误材质。
- H-03 不创建 Scene、Camera 或 Light 对象；调用方可以逐 Draw 显式提供输入。
- 阴影、IBL 和后处理不以临时隐藏实现混入 H-03。

## H-03A1 实现记录

`material_variant` 现拥有顶点 Buffer/Attribute 布局、Primitive、Depth 和单颜色 Attachment Blend
状态。包构建阶段限制 Buffer/Attribute 数量，验证格式、stride、offset、location 唯一性以及所有
固定状态枚举和保留字段。`material_template_gpu` 将拥有数据转换为调用期间有效的 C API 描述，
实际创建的 Graphics Pipeline 不再依赖隐藏默认状态。

现有 `.grmat` v1 尚未编码这些字段。编码器遇到任何非默认 Pipeline 状态会返回明确错误，不能生成
丢失信息的包；默认状态包仍保持原有逐字节结果。H-03A2 将直接提升开发期格式版本并加入持久化
记录，不提供尚未承诺的 v1 自动迁移。

## H-03A2a 实现记录

材质包与归档主版本已提升到 2，并新增必需 `pipeline_states` 区段。区段使用独立的 State、Vertex
Buffer 和 Vertex Attribute 定宽记录；所有关联通过有界索引表达，解码器在分配前检查数量、记录
尺寸、区段精确长度、索引范围和保留字段。相同输入仍生成确定性字节流。

同一 Pass 的所有 Variant 现在必须具有完全相同的 Pipeline 状态。v1 属于未发布开发格式，v2
读取器明确拒绝旧主版本，不增加迁移分支。源 JSON fixture 已同步切换版本号。

## H-03A2b 实现记录

源 JSON 现在可描述顶点布局、图元、深度和单颜色 Attachment 混合状态。枚举使用稳定的可读名称，
混合的细分字段可省略并继承默认值；未知名称和非法数值会在构建阶段失败。调试 JSON 同步导出完整
状态，除颜色写掩码外不再暴露难以阅读的枚举数值。H-03A 至此闭环，下一步进入 H-03B 的 CPU
参考 BRDF 与固定测试向量。

## H-03B 实现记录

`pbr_reference` 提供不依赖 Vulkan 的 CPU 数学基准，包含 Schlick Fresnel、GGX 法线分布、Smith
correlated GGX 可见性和单方向光直接光照。输入方向会归一化；Base Color 以线性 RGB 表达并限制
为非负值，Metallic 限制到 `[0, 1]`，Perceptual Roughness 限制到 `[0.045, 1]`。介电材质
`F0` 固定为 `0.04`，输出为未经过曝光和 Tone Mapping 的线性 RGB radiance。

测试记录了正入射介电材质、正入射金属材质和彩色半金属三组固定数值，并覆盖背光和非法参数边界。
该实现只承担规范与测试基准，不进入实时逐像素路径；H-03C 的 HLSL 实现必须复用同一公式语义，
并与这些向量或离屏输出对照。

## H-03C 实现记录

`assets/shaders/pbr/pbr_untextured.hlsl` 已按 H-03B 的公式实现 Schlick、GGX 和 Smith correlated
GGX，并使用固定的线性 Base Color、Metallic、Perceptual Roughness 与正入射方向光。顶点阶段
通过 `SV_VertexID` 生成三角形，首版不要求 Vertex Buffer；材质常量与纹理绑定留到 H-03D。

示例资产同时提供源 HLSL、DXC 生成的 Vulkan 1.3 SPIR-V 和 `.grmat.json` 离线包描述。源描述可由
`granit_material_tool` 确定性构建为 v2 二进制包。`granit_pbr_offscreen_example` 通过
`material_template_gpu` 创建真实 Pipeline，在 RGBA8 颜色与 D32 深度附件上完成 Draw、提交和安全
回收；示例已注册为 CTest。当前 Renderer 尚无 Texture 到 Readback Buffer 的公开复制接口，因此
本阶段验证真实执行成功，像素级 CPU/GPU 数值回归保留到 H-03F。

## H-03D1 实现记录

无纹理 PBR Shader 不再硬编码材质值。Group 1、Binding 0 的 48 字节常量块现包含线性
`base_color`、`metallic`、`perceptual_roughness`、`normal_scale`、`occlusion_strength` 和线性
`emissive`；字段偏移同时记录在 `.grmat.json` 元数据中。示例使用 `material_gpu_instance` 按稳定
参数 ID 更新 CPU shadow buffer、批量上传 Uniform Buffer、创建 Bind Group，并在 Draw 前绑定到
`material_template_gpu` 的 Pipeline Layout。

本阶段先固定无纹理路径，但已经保留 Normal Scale 与 Occlusion Strength 的统一常量布局，避免
H-03D2 接入纹理时再次改变常量 ABI。

## H-03D2a 实现记录

PBR 纹理变体统一使用 `pbr_texture_mask` 位掩码，五个位依次表示 Base Color、
Metallic/Roughness、Normal、Occlusion 和 Emissive。Group 1 固定为 Binding 0 常量块、Binding
1～5 五类纹理和 Binding 6 共享采样器；后续增加变体不会改变 Pipeline Layout。

标准网格 location 固定为 0 Position、1 Normal、2 Tangent、3 UV0。无纹理路径只要求 Position 和
Normal；任意纹理要求 UV0，Normal Texture 额外要求带 handedness 的 Tangent。新增纯 CPU 校验会
返回具体缺失项，并拒绝未知 feature 位。该检查不从 location 猜测更高层语义，而是把 location
映射正式定义为 H-03 模块契约。

## H-03D2b 实现记录

`pbr_default_resources` 创建五张 1×1 纹理和一个线性采样器：Base Color 与 Emissive 使用 sRGB
格式，其余数据纹理使用线性 UNORM；白色保持 Base Color、Metallic/Roughness、Occlusion 和
Emissive 乘数，法线纹理使用近似 `(0.5, 0.5, 1.0)`。默认资源可一次性写入
`material_gpu_instance`，保证没有用户资产时 Bind Group 仍完整有效。

PBR Shader 现在真实采样五类纹理：Base Color 和 Emissive 按颜色纹理处理，Metallic/Roughness
使用 B/G 通道，Normal 通过 TBN 与 Normal Scale 转换，Occlusion 按 Strength 混合。当前示例使用
`pbr_texture_mask = 31` 的全纹理变体和默认资源完成离屏 Draw；H-03D2c 再生成按纹理组合裁剪的
Shader 变体，避免未启用槽位的无效采样。

实现过程中补齐了 Renderer 的 Graphics Bind Group 资源状态跟踪。图形描述符中的 Uniform、
Storage Buffer、Sampled/Storage Texture 现在会传入 Recorder，在提交前生成访问屏障与 Image
Layout 转换；因此刚完成上传的默认纹理能从 `TRANSFER_DST` 正确进入
`SHADER_READ_ONLY_OPTIMAL`，无需调用方接触 Vulkan 同步。

## H-03D2c 实现记录

HLSL 使用编译期 `GRANIT_PBR_TEXTURE_MASK` 控制纹理声明、采样和 TBN 分支，未启用的槽位不会进入
最终 SPIR-V。仓库固定携带 mask 0 与 mask 31 两个边界变体：前者反射结果只有 48 字节常量块，
后者包含五张纹理和共享采样器；离线 `.grmat` 包同时记录两个稳定变体键。

中间组合使用同一 HLSL 源按实际资产需求传入 `-D GRANIT_PBR_TEXTURE_MASK=<mask>` 编译，不默认
生成全部 32 份 SPIR-V，避免材质包无条件膨胀。所有组合仍共享相同的 Group 1 Pipeline Layout，
默认资源保证使用者切换变体时不需要临时构造占位资源。H-03D 至此完成，下一步进入 H-03E 的
View/Object/Directional Light 显式输入与 Render Graph Pass 适配。

## H-03E1 实现记录

`pbr_draw_inputs` 定义调用方显式提供的 View、Object 和单方向光数据，不保存 Camera、Transform、
Light 对象或场景集合。矩阵采用与 HLSL `column-major float4x4` 一致的 16 个 float 列主序布局；
打包结果固定为 112 字节 Frame 常量和 144 字节 Object 常量，分别对应 Group 0 与 Group 2。

打包阶段检查所有浮点值有限、方向光非零且 radiance 非负，并将光照方向规范化。Object ID 写入
独立的 16 字节槽，避免 C++/HLSL 常量布局差异。纯 CPU 测试覆盖正常打包、方向规范化、负光照、
零方向和非有限值。H-03E2 将先把 Render Graph 原型整理为独立 CMake 目标，再由单独的 PBR Pass
适配层依赖它，避免 `granit_material` 直接绑定某个图执行器。

## H-03E2 实现记录

串行 Render Graph 已从测试和 benchmark 各自重复编译源码，整理为内部静态目标
`granit::render_graph`。测试和 benchmark 统一链接该目标，后续图实现演进不再产生多份构建定义。
它仍不是核心 `granit` 动态库的传递组成，也尚未安装导出。

新增独立 `granit::pbr` 目标，依赖 `granit::material` 与 `granit::render_graph`。其
`add_pbr_graph_pass` 会验证并复制显式输入、打包 Frame/Object 常量、声明颜色与可选深度附件写访问，
再将常量交给调用方提供的录制回调。适配器不保存 Scene、Camera、Light 集合，不决定 Mesh 来源，
也不隐藏实际 Draw。测试覆盖附件解析、方向规范化、对象常量传递和不完整描述拒绝。H-03E 至此
完成，下一步进入 H-03F 的端到端回归、生命周期与性能基线。

## H-03F1 实现记录

固定 CPU BRDF 向量继续作为材质语义的数值回归基准。PBR Render Graph 测试新增所有权边界：
`add_pbr_graph_pass` 在加入图时复制并打包 View、Light 和 Object 输入，调用方随后修改原始描述不会
改变已记录 Pass；录制回调及其捕获由 Graph 持有，并随 Graph 销毁释放。导入的 Texture View 仍由
调用方持有，适配器不销毁外部资源。

`granit_pbr_benchmarks` 建立纯 CPU 基线，覆盖 100 个 Object 的输入复制、常量打包、Pass 加入与
图编译。首份 Windows Clang Release 基线 P50 为 7.402 us/Pass、P95 为 7.573 us/Pass。该结果不
包含 GPU 录制、提交或 Draw，只用于同环境回归；对象布局、批量打包或 Graph 存储方式变化，以及
P50/P95 稳定退化超过 10% 时应复测。

H-04/H-05 必须沿用以下输入契约：

- View 与 Object 继续由调用方逐 Pass 显式提供，不引入 Scene、Camera 或 Transform 所有权。
- H-04 可将单方向光扩展为显式光源数组或光源 Buffer，但不得改变 H-03 常量及 Group 0/2 的含义；
  光源筛选和分块/聚簇结果应作为独立 Pass 资源。
- H-05 的阴影、IBL、探针和 Pass 数据使用 Group 3 或独立 Render Graph 资源，不向 PBR 材质 Group 1
  塞入场景级状态。
- 新增高层模块持有自己的资源和缓存；`granit::pbr` 只消费句柄与值数据，核心 Renderer 不反向依赖
  PBR、Scene 或 Render Graph。

H-03F2a 先补齐 Texture 到 Readback Buffer 的公开复制能力；H-03F2b 再用固定离屏场景比较 Shader
像素与 CPU 参考值。在 H-03F2b 完成前，当前端到端测试只证明真实 Draw、提交和资源回收成功。

## H-03F2a 实现记录

Command Recorder 新增 Texture 到 Buffer 的显式复制命令，复用 Texture 写入已有的数据布局与区域
结构。公共层校验资源所属 Renderer、usage、格式、单采样限制、mip/array/三维范围、行跨度、目标
Buffer 容量和 offset 对齐；Vulkan 后端通过统一状态跟踪将源 Image 转为
`TRANSFER_SRC_OPTIMAL`，并跟踪目标 Buffer 的传输写入。

集成测试将固定 2x2 RGBA8 数据写入 Texture，经命令复制到 readback Buffer，等待 Recorder 完成后
映射并逐字节比较。该测试闭合了通用读回基础设施；H-03F2b 仍需把相同步骤接入 PBR 离屏颜色目标，
并定义线性输出量化和像素误差范围。

## H-03F2b 实现记录

PBR 离屏示例的 RGBA8 颜色目标现带 `TRANSFER_SOURCE` usage。绘制结束后，同一 Recorder 将完整颜色
目标复制到 readback Buffer；提交完成后检查三角形中心像素与覆盖外的清屏像素。中心参考值由 CPU
BRDF 使用相同材质参数和默认法线纹理值计算，再量化到 UNORM8；RGB 与 Alpha 允许最多 2 个整数级
误差，清屏色允许 1 个整数级误差。

首次启用读回时，回归发现原示例的 `counter_clockwise + back cull` 会在当前正高度 Vulkan viewport
下剔除整个三角形。PBR Pipeline 与源材质描述已改为 clockwise front face，像素测试因此同时覆盖
实际图元覆盖、默认纹理采样、PBR Shader 数值和颜色附件存储。H-03 首版至此完成。
