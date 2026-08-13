<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-05：光照与后处理参考管线

## 状态

- 路线图任务：H-05
- 优先级：P2
- 状态：实现中，H-05A 已完成
- 依赖：H-01 Render Graph、H-02 Material、H-03 PBR、H-04 Scene Submission

## 背景

H-03 已闭合单方向光、不透明前向 PBR，H-04 已提供逐 View 的可见对象和方向光、点光、聚光索引。
H-05 在这些值模型之上建立一条可运行、可测试的参考光照管线，覆盖多光源、方向光阴影、环境光照、
HDR 合成和 Tone Mapping。它用于验证 Granit 的资源、材质和 Render Graph 能力，不成为唯一允许的
渲染架构。

## 目标

- 支持方向光、点光和聚光的确定性前向 PBR 计算。
- 提供首版单方向光阴影，并为级联阴影保留扩展位置。
- 支持基于图像的漫反射与镜面反射环境光照。
- 使用 HDR 中间颜色目标，并通过显式曝光和 Tone Mapping 输出显示颜色。
- 使用 Render Graph 表达阴影、主光照和后处理之间的资源依赖。
- 建立 CPU 数值参考、GPU 像素回归、生命周期测试和性能基线。

## 非目标

- 不在首版实现延迟渲染、路径追踪、透明物体、自动曝光、Bloom 或抗锯齿。
- 不在首版实现点光/聚光阴影、反射探针混合、天空大气或运行时环境贴图烘焙。
- 不预先实现分块、聚簇、Bindless、GPU culling 或内部任务系统。
- 不负责加载 HDR 图片、材质包、Mesh 或场景文件；这些属于调用方或后续 Asset 模块。
- 不把 H-05 接口加入稳定 C ABI，也不让核心 Renderer 反向依赖高层模块。

## 模块与所有权

首版新增可选内部目标 `granit::lighting` 和 `granit::post_process`：

- `granit::lighting` 依赖 Scene、PBR 和 Render Graph，负责光源打包、阴影与 IBL Pass 适配。
- `granit::post_process` 只依赖 Render Graph 和核心 Renderer，负责 HDR 到显示颜色的转换。
- Scene 快照、Renderable payload、Mesh 和 Material 仍由调用方持有。
- 环境 Texture View、Sampler 和可选 BRDF LUT 由调用方创建并持有；H-05 只复制句柄和值描述。
- H-05 创建的帧内 Attachment 和 Pass 数据由对应 Graph/执行上下文持有，不写回 Scene。
- 长期缓存（例如阴影图或预过滤环境图）只有在重复帧测量证明有必要后才引入。

核心依赖保持单向，箭头表示“左侧依赖右侧”：

```text
lighting -> scene / pbr / render_graph -> renderer
post_process -> render_graph -> renderer
```

## 首版光源模型

每个 View 从 `multi_view_snapshot` 读取已经通过层掩码和粗粒度范围筛选的光源索引，再打包为独立
GPU Buffer。首版采用明确容量的有界数组：

- 方向光默认最多 4 个。
- 点光默认最多 64 个。
- 聚光默认最多 64 个。
- 容量由管线创建描述配置，但不得超过实现公布的能力上限。
- 超出容量时返回明确错误和实际需求数量，不静默截断或自动选择“最重要”的光。

点光使用平方反比衰减，并在显式作用半径附近平滑衰减到零；聚光额外使用内外锥角余弦之间的
平滑响应。CPU 参考实现与 Shader 必须共享相同公式、单位和边界行为。方向光、点光和聚光的 GPU
布局独立于 H-04 输入结构，避免为了 Shader 对齐污染 Scene 值模型。

首版直接循环当前 View 的有界光源数组。只有在目标多光源场景中，光照 Pass 的 GPU 时间或每像素
循环稳定成为瓶颈后，才重新评估分块或聚簇光照；届时筛选结果作为新的 Render Graph 资源，不改变
Scene 所有权。

## 阴影方案

H-05B 首先实现一盏由调用方明确指定的方向光、单个正交投影阴影图：

- 阴影图使用单采样 `D32_FLOAT`，首版尺寸由创建描述显式给出。
- Shadow Pass 只消费可投射阴影的 Renderable，调用方回调负责绑定深度 Pipeline、Mesh 并 Draw。
- 主光照 Pass 通过 Group 3 读取阴影 Texture View、比较 Sampler、light view-projection 和偏移参数。
- 材质 Group 1 不保存阴影图；Object Group 2 不保存场景级阴影状态。
- 调用方必须明确指定投射阴影的方向光，不按数组顺序隐式选择第一盏。
- 未配置阴影时使用无阴影路径，不为每个像素绑定伪造的材质资源。

首版验证稳定后再增加级联方向光阴影。级联拆分、稳定化和缓存作为 H-05B2，不能改变单级阴影的
资源归属。点光与聚光阴影留作后续任务。

## IBL 方案

首版采用常见的 split-sum 金属度/粗糙度环境光照：

- 漫反射辐照度立方体贴图。
- 按粗糙度预过滤的镜面反射立方体贴图及 mip 链。
- 二维 BRDF 积分 LUT。
- 显式环境强度和旋转参数。

H-05 只消费已经生成的 GPU 资源，不负责解析图片或离线卷积。缺少环境输入时使用零环境贡献，
不能让未配置 IBL 阻止直接光照。环境资源属于 View/Pass 数据并绑定到 Group 3，不属于单个材质。
反射探针、局部体积和探针混合不进入首版。

## HDR 与 Tone Mapping

主光照输出 `RGBA16_FLOAT` HDR 颜色，深度继续使用 `D32_FLOAT`。首版后处理使用显式曝光和
ACES fitted Tone Mapping，不实现自动曝光。输出转换必须明确区分：

- sRGB Attachment：Shader 输出线性显示颜色，由 Attachment 完成 sRGB 编码。
- UNORM Attachment：Shader 按配置显式执行线性到 sRGB 编码。

管线创建时同时校验目标格式和输出传递模式，拒绝会导致重复编码或完全不编码的组合。离屏像素
回归优先使用 `RGBA8_UNORM` 加显式 sRGB 编码；窗口路径优先选择 sRGB Swapchain 格式。

## Pass 顺序与资源关系

首版每个 View 的逻辑顺序如下：

```text
Shadow Depth Pass（可选）
  -> Forward PBR HDR Pass（直接光 + 阴影 + IBL）
  -> Tone Mapping Pass
  -> 最终颜色目标
```

阴影深度、HDR 颜色和最终输出都通过 Render Graph 声明访问关系。H-05 不直接插入 Vulkan 屏障，
也不绕过 Command Recorder。多 View 默认各自拥有 Pass 和中间目标；共享阴影或探针缓存需要后续
以显式资源导入表达。

## 绑定约定

沿用 H-02/H-03 已确认分组：

- Group 0：Frame/View 常量。
- Group 1：Material 参数与纹理。
- Group 2：Object 常量。
- Group 3：光源 Buffer、阴影、IBL 和其他 Pass 级资源。

H-05 可以扩展 Group 3，但不得改变 Group 1 的材质语义。Bindless 仍是 Renderer 的可选能力，
H-05 首版必须能在普通 Bind Group 路径运行。

## 线程与生命周期

- H-05 构建函数读取不可变 `multi_view_snapshot`，不同 View 可由外部执行器并行准备。
- 首版模块不创建线程池，也不长期保存调用方快照地址。
- 加入 Graph 时复制帧常量、光源值和回调；外部 Texture View/Sampler 只复制句柄。
- 外部资源必须至少存活到 Graph 执行完成；销毁顺序错误由现有句柄校验和生命周期诊断报告。
- 任一步验证或分配失败都不得留下半成品 Pass 或覆盖上一份有效管线状态。

## 实施顺序

### H-05A：多光源数据与数值参考

- 定义方向光、点光、聚光的 CPU/GPU 打包结构和容量描述。
- 实现衰减、聚光锥响应及直接光 BRDF 的 CPU 参考。
- 将 H-04 可见光源索引转换为确定性的逐 View 光源 Buffer 输入。
- 测试边界值、非法参数、容量溢出、所有权和多 View 隔离。

### H-05B：方向光阴影

- 实现单方向光 Shadow Pass、深度 Attachment 和 Group 3 阴影输入。
- 增加 bias、normal bias、比较采样和阴影边界数值测试。
- 完成离屏阴影像素回归后，再实施 H-05B2 级联阴影。

### H-05C：环境光照

- 接入 irradiance、prefiltered environment 和 BRDF LUT。
- 增加缺省零环境、粗糙度 mip 选择和金属/非金属回归。
- 使用固定生成数据测试，不把图片解码库引入 H-05。

## H-05C0 实现记录

IBL 开始前确认 Renderer 原先虽有 Cube 枚举，但验证和 Vulkan View 仍固定为 2D 单 mip。现已补齐
单个六面 Cube Texture/View、连续 mip View、默认完整范围 View 和指定 face/mip 写入。Vulkan Image
使用 cube-compatible 标志，View 使用 cube 类型；校验要求正方形、六层、单采样且 mip 不超过完整
链，并验证 View 子资源不越过父 Texture。

真实 Renderer 测试已创建 `RGBA16_FLOAT`、8x8、四级 mip 的 Cube，建立覆盖六面的 Cube View，
并写入第六面第二级 mip。该能力只解决 H-05C 的资源基础，环境卷积和 mip 生成仍属于调用方或后续
离线 Asset 工具。

## H-05C1 实现记录

已增加 split-sum IBL 的 CPU 数值参考，明确首版输入是调用方预先生成并采样的 irradiance、
prefiltered environment 和 BRDF LUT，而不是在运行时解析或卷积环境图片。漫反射按 Lambert 项除以
pi，镜面反射使用预过滤环境与 LUT 的缩放/偏移组合；金属度决定漫反射占比，AO 只调制间接光，
环境强度小于零时按零处理。

粗糙度在 `[0, 1]` 内线性映射到最大 mip 索引，并增加绕世界 Y 轴的环境查询方向旋转。固定数值测试
覆盖 mip 边界、九十度旋转、金属/非金属差异、缺省零环境和完全遮蔽。下一步建立 Group 3 环境资源
布局并让 Shader 与该 CPU 参考对照。

## H-05C2 实现记录

已建立 IBL 的 Group 3 资源对象。为与现有阴影资源共存，阴影固定占用 binding 0～2，IBL 固定占用
binding 3～7，依次为环境常量、irradiance Cube、prefiltered environment Cube、BRDF LUT 和共享
线性 Sampler。环境常量保存旋转的正弦/余弦、非负强度和预过滤环境最大 mip 索引。

资源对象拥有常量 Buffer、Sampler、布局和 Bind Group，但只借用调用方持有的三个 Texture View。
真实 Vulkan 测试覆盖两个四级 mip Cube、二维浮点 LUT、常量更新和不完整输入拒绝。当前提供
IBL-only 布局；阴影与 IBL 组合 Shader 将使用同一编号契约建立包含 binding 0～7 的统一 Group 3，
不会尝试在同一 Pipeline 组号同时绑定两个 Bind Group。

## H-05C3 实现记录

PBR HLSL 已增加 `GRANIT_PBR_IBL` 编译变体，并保留与 `GRANIT_PBR_SHADOWS` 任意组合的能力。
IBL Shader 使用 Group 3 binding 3～7，与阴影 binding 0～2 组成统一且无冲突的 Pipeline 布局；组合
变体不会在同一组号绑定两个 Bind Group。

Shader 计算与 CPU 参考保持一致：漫反射 irradiance 除以 pi，镜面反射按粗糙度选择预过滤 Cube mip
并结合 BRDF LUT；环境旋转同时作用于法线和反射查询方向，AO 只调制间接光，不再错误影响直接光。
本阶段以 SPIR-V 反射固定 IBL-only 和阴影加 IBL 的资源契约。下一步用固定生成纹理建立统一 Group 3
Bind Group 和离屏像素回归。

## H-05C4 实现记录

已增加阴影与 IBL 共用的 Group 3 资源对象，统一持有两个常量 Buffer、比较 Sampler、环境线性
Sampler、完整 binding 0～7 布局及一个 Bind Group；四个 Texture View 继续由调用方持有。阴影和
环境常量可独立更新，不需要重建 Bind Group。

离屏 PBR 回归现使用程序生成的六面 `RGBA16_FLOAT` irradiance/prefiltered Cube 和二维 BRDF LUT，
不依赖图片解码或离线资源。遮挡用例验证直接光归零但 IBL 保留，受光用例验证直接光与 IBL 相加，
两者均与 CPU BRDF/IBL 参考值比较。H-05C 首版至此完成，下一步进入 H-05D HDR 与 Tone Mapping。

### H-05D：HDR 与 Tone Mapping

- 建立 HDR Attachment 和 ACES fitted CPU 参考。
- 实现曝光、输出传递模式与格式组合校验。
- 覆盖黑色、负值防护、过曝、高亮和 sRGB 编码像素回归。

## H-05D1 实现记录

已建立曝光与 Tone Mapping 的 CPU 数值参考。曝光使用 `2^EV` 在曲线前缩放 HDR，首版显式接受
`[-24, +24] EV`；非有限颜色或越界曝光返回错误且不修改输出。负 HDR 分量在 ACES fitted 近似前
归零，曲线结果限制到 `[0, 1]`。

输出传递模式明确区分 sRGB Attachment 自动编码和 UNORM Attachment 下的 Shader 显式 sRGB 编码，
格式与模式不匹配时拒绝创建后处理路径，防止重复编码或漏编码。数值测试覆盖黑色、负值、标准亮度、
高亮、曝光、sRGB 分段边界及失败不修改。下一步建立 HDR Attachment 和 Tone Mapping Shader Pass。

## H-05D2 实现记录

已增加 `RGBA16_FLOAT` 瞬态 HDR Attachment 描述及 Tone Mapping Render Graph 适配器。Pass 明确声明
HDR 纹理只读、最终颜色目标只写，并复制曝光缩放与输出编码常量；资源缺失、输入输出别名、曝光越界
或格式/传递模式误配时拒绝加入 Graph。物理 HDR Texture 由现有 Render Graph 在首次使用时创建、
末次使用后释放，不需要 H-05 绕过 Graph 管理 Vulkan 资源。

全屏三角形 Tone Mapping Shader 使用 Group 0 的 16 字节常量、HDR Texture 与线性 Sampler，支持
ACES fitted 和可选 Shader sRGB 编码。SPIR-V 已通过 Vulkan 1.3 校验并增加资源反射测试。下一步建立
GPU 资源对象和真实 HDR 到 UNORM/sRGB 输出的离屏像素回归。

### H-05E：完整参考管线与测量

- 串联 Shadow、Forward PBR 和 Tone Mapping Pass。
- 覆盖无阴影、无 IBL、多 View、窗口与离屏路径。
- 建立 1/16/64/128 个可见光源的 CPU 打包和 GPU 帧时间基线。
- 仅依据测量结果决定是否建立后续分块/聚簇光照任务。

## H-05A 实现记录

新增可选内部目标 `granit::lighting`，首版依赖 `granit::scene`，不进入核心动态库或安装导出。
模块定义 16 字节对齐的方向光、点光和聚光 GPU 元素；点光与方向光均为 32 字节，聚光为 48 字节。
聚光内外角在打包时转换为余弦，Shader 无需逐光计算三角函数。

`pack_view_lights` 消费 H-04 的逐 View 可见索引，按原有确定性顺序复制对应光源。默认容量为
4/64/64，实现上限为 16/256/256；容量不足会返回三类真实需求数量并保留旧输出，非法实现上限、
View 越界和分配失败均有独立错误。

CPU 参考实现沿用 H-03 的金属度/粗糙度 BRDF。点光采用带平滑半径截止的平方反比衰减，聚光在此
基础上增加内外锥角之间的 smoothstep 响应。测试已覆盖布局字段、角度转换、容量事务语义、半径
边界、聚光过渡、范围外归零以及点光/聚光直接光结果。下一步进入 H-05B 单方向光阴影。

进入 H-05B 前新增了最小内部 `granit::math` 目标，统一 Scene、PBR 和 Lighting 已重复出现的
`float3` 与列主序 `matrix4`。数学层提供右手 `look_at`、Vulkan `[0,1]` 深度的透视/正交投影、
矩阵乘法和点/向量变换；现有模块名称保留为类型别名，避免无意义的调用方迁移。阴影实现应直接
使用这套矩阵约定，不再增加局部数学类型或重复函数。

## H-05B1 实现记录

新增单方向光阴影描述与 Render Graph 适配器。调用方通过全局方向光索引显式选择投影光源；适配器
确认该光源对目标 View 可见，并用配置的焦点、正交范围、近远面和光源距离构建右手光源矩阵。
当光线接近世界 Y 轴时会自动改用 X 轴作为 up，避免 `look_at` 退化。

投影者不会复用相机可见列表，而是使用光源 view-projection 从全部 Renderable 重新执行 Frustum
筛选，并同时应用目标 View 与光源层掩码。Pass 描述复制 Model、payload、Object ID 和源索引，
因此 Graph 执行不依赖原快照地址；失败不覆盖旧描述。Shadow Pass 当前只声明外部或瞬态深度资源
的写访问，实际深度 Pipeline、Mesh 绑定和 Draw 由录制回调负责。

测试覆盖独立阴影视锥、确定性投影者顺序、不可见光源、非法正交体、无效深度资源、输入复制、
Graph 深度写生命周期及真实 Recorder 执行。H-05B 尚未完成；下一步接入固定深度 Pipeline、深度
bias、比较采样和主 PBR Pass 的 Group 3 阴影输入。

## H-05B2a 实现记录

Graphics Pipeline 描述新增可选固定 depth bias，包含 constant factor、slope factor 和非负 clamp。
C API 通过尾部字段与 V5 `struct_size` 扩展，旧描述保持关闭 bias；C++20 包装使用 `optional`。
后端直接写入 Vulkan Rasterization State，拒绝非有限值、负 clamp 和非零保留字段。

比较 Sampler 已由现有 API 支持，不需要新增 Vulkan 暴露。当前选择固定 Pipeline bias，而不是立即
增加 Recorder 动态命令；阴影路径可先用少量缓存 Pipeline 验证参数，只有真实材质/级联数量证明
Pipeline 组合成为问题时再引入动态状态。H-05B2 下一步继续建立 Shadow Group 3 布局、比较 Sampler
和主 PBR Pass 的阴影采样常量。

## H-05B2b 实现记录

新增 `shadow_resources`，拥有 Group 3 的 80 字节采样常量 Buffer、`less_equal` 比较 Sampler、
Bind Group Layout 和不可变 Bind Group；阴影 D32 Texture View 由调用方持有。常量包含光源
view-projection、接收端 depth/normal bias 和 texel size，更新会拒绝非有限值、负 bias 或非正
texel size。销毁顺序先释放 Bind Group，再释放布局、Sampler 和 Buffer。

Material GPU 模板允许在既有 Group 0/1 后追加最多六个高层布局。H-05 使用一个占位或真实 Object
Group 2 后追加 Shadow Group 3；Material 只组合调用方提供的布局，不理解阴影语义。PBR Render
Graph Pass 新增可选阴影资源读访问，使 Shadow 写入与主光照采样产生明确依赖。

真实 Renderer 测试覆盖 D32 sampled/depth Texture、比较 Sampler、Group 创建、常量更新和资源回收；
Material 测试覆盖 Group 2/3 追加及空句柄拒绝。下一步修改 PBR Shader 变体实际读取 Group 3，并
增加有遮挡/无遮挡的离屏像素回归。

## H-05B2c 实现记录

PBR HLSL 新增编译期开关 `GRANIT_PBR_SHADOWS`。关闭时不声明阴影输入，保持现有无阴影材质布局；
开启时 Vertex Shader 向 Fragment Shader 传递世界位置，Group 3 声明 80 字节 Shadow 常量、D32
采样纹理和 `SamplerComparisonState`。Fragment Shader 应用 normal bias，转换到 Vulkan `[0,1]`
深度与纹理 Y 方向，范围外按完全受光处理，再用 depth bias 执行 `SampleCmpLevelZero`。

仓库新增无纹理、全纹理 Fragment 以及配套 Vertex 的阴影 SPIR-V。反射工具确认资源位于 set 3 的
binding 0/1/2，常量大小为 80 字节。CPU 参考实现与 Shader 使用相同投影、范围和 `less_equal`
比较语义，测试覆盖受光、遮挡、Y 翻转、范围外和非法输入。下一步将该变体接入真实离屏场景，
比较有遮挡与无遮挡像素后完成 H-05B。

## H-05B2d 实现记录

现有 PBR 离屏示例已切换到阴影 Shader 变体，并创建 1x1 `D32_FLOAT` 深度/采样 Texture、Group 3
资源以及 Material Pipeline Layout 的空 Object Group 2。每次回归先将阴影 Attachment 清除为固定
深度，再由状态跟踪自动转换为 sampled 访问，随后绑定 Material Group 1 与 Shadow Group 3 绘制
同一三角形并读回颜色。

测试连续执行两种确定性情况：阴影图深度 0.25 小于接收者深度 0.5，中心 RGB 必须为零；阴影图
深度 1.0 通过 `less_equal` 比较，中心 RGB 必须与 H-03 CPU BRDF 参考值在两个 UNORM8 级别内一致。
两次都验证覆盖外清屏像素，证明结果不是未绘制造成。H-05B 单方向光阴影首版至此完成；级联阴影
保留为基础参考管线完成后的独立扩展，主线下一步进入 H-05C IBL。

## 测试与验收

- C++ 单元测试覆盖所有光源公式、容量、格式和失败事务语义。
- CPU 与 Shader 使用固定向量比较直接光、IBL 和 Tone Mapping 数值。
- GPU 像素回归至少覆盖单点光、单聚光、方向光阴影、IBL 和高亮 Tone Mapping。
- Render Graph 测试确认 Pass 顺序、资源访问和可选路径不会产生虚假依赖。
- 生命周期测试确认快照、回调和外部环境资源的所有权边界。
- 核心 `granit::granit` 仍可在不构建 H-05 模块时独立构建、测试和安装。
- 公共头文件和安装导出不出现 Vulkan 类型或 H-05 内部实现依赖。

## 重新评估条件

满足任一条件时建立单独计划评估分块/聚簇光照：

- 目标场景中每 View 经 H-04 粗筛后经常超过 64 个点光或 64 个聚光。
- 有界前向光源循环稳定占据主要 GPU 帧时间，且降低材质或分辨率后瓶颈仍存在。
- 多 View 重复构建光源数据成为可测 CPU 瓶颈。
- Bindless 已具备真实使用场景，并能显著简化光源或探针资源索引。

自动曝光、Bloom、透明排序、点光/聚光阴影、探针混合和环境图生成均应作为独立后续任务，不在
H-05 实施过程中顺带扩张范围。
