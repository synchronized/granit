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

## H-05D3 实现记录

已增加 Tone Mapping GPU 资源对象，拥有 16 字节常量 Buffer、线性 Sampler、Group 0 Bind Group、
Pipeline Layout、顶点/片元 Shader 和目标格式专用 Graphics Pipeline。资源对象借用 HDR Texture View，
支持更新曝光缩放与 sRGB 编码开关，并按依赖逆序销毁全部 GPU 对象。

真实 Vulkan 测试已覆盖 `RGBA16_FLOAT` HDR View 到 `RGBA8_UNORM` Pipeline 的完整创建、常量更新和
非法输入拒绝。最初回读全零的原因已定位为在异步 `submit()` 后、GPU 完成前直接映射 Readback
Buffer；改为在映射前通过 `reset()` 等待该 Recorder 完成后，固定 HDR 高亮值经 `+1 EV`、ACES 和
Shader sRGB 编码得到的 GPU 像素与 CPU 参考逐通道一致。H-05D 首版 Tone Mapping 像素闭环完成。

## H-05D4 实现记录

Tone Mapping GPU 资源层现强制校验目标格式与编码开关：`RGBA8_UNORM/BGRA8_UNORM` 只接受 Shader
显式 sRGB 编码，`RGBA8_SRGB/BGRA8_SRGB` 只接受线性 Shader 输出并由 Attachment 编码。重复编码、
漏编码及其他首版不支持的最终格式都会在创建 Buffer、Shader 或 Pipeline 前返回参数错误。真实 Vulkan
测试覆盖两种误配拒绝及 sRGB Attachment Pipeline 成功创建，确保 D1 的传递模式契约落实到 GPU 层。

## H-05D5 实现记录

离屏 PBR 示例现使用 `RGBA16_FLOAT` 作为主光照目标，方向光阴影、直接光和 split-sum IBL 先在线性
HDR 空间合成，再由 Tone Mapping 全屏 Pass 输出到 `RGBA8_UNORM`。最终像素回读同时与 CPU PBR、
IBL、曝光、ACES fitted 和 sRGB 参考结果比较，覆盖受光与遮挡区域，不再只分别验证 PBR 和后处理。

示例在读取 Readback Buffer 前显式等待 Recorder 完成，并在销毁借用的 HDR Texture View 前释放
Tone Mapping 资源。H-05D 至此完成，下一步进入 H-05E 的完整参考管线组合、降级路径与性能测量。

### H-05E：完整参考管线与测量

- 串联 Shadow、Forward PBR 和 Tone Mapping Pass。
- 覆盖无阴影、无 IBL、多 View、窗口与离屏路径。
- 建立 1/16/64/128 个可见光源的 CPU 打包和 GPU 帧时间基线。
- 仅依据测量结果决定是否建立后续分块/聚簇光照任务。

## 剩余技术验证顺序

H-05E 从当前状态按以下顺序收敛，不在完成这些验证前继续增加新的光照模型或后处理效果：

1. **GPU 时间戳与性能基线（已完成）**：在 Renderer 增加不暴露 Vulkan 类型的时间戳查询能力，分别测量
   Shadow、1/16/64/128 光源 PBR 和 Tone Mapping；严格区分 CPU 录制/提交等待与 GPU 执行时间。
2. **功能降级组合（已完成）**：验证零光源、仅直接光、仅 IBL、无阴影、无 IBL 和完整组合；不通过伪造
   Texture 或无意义占位资源掩盖缺失能力。
3. **窗口与 Swapchain 闭环（已完成）**：将 HDR 中间目标输出到窗口，验证 sRGB 传递、Resize、最小化、
   Swapchain 重建和离屏/窗口共用 Pass 模型。
4. **多 View 完整渲染**：每个 View 使用独立可见光源、光源 Buffer、HDR/Depth/输出目标，验证
   连续录制与提交，同时不复制整份 Scene 数据。
5. **Render Graph 统一组合**：由 Graph 串联可选 Shadow、PBR HDR 和 Tone Mapping，验证瞬态资源、
   屏障、失败回滚及多帧重复执行，不再由示例长期手工维护整条命令链。
6. **多帧生命周期稳定性**：连续运行至少数千帧，覆盖帧间 View/光源/材质更新、Resize、GPU 在途
   资源延迟销毁、Renderer 销毁诊断和 Device Lost 错误传播。
7. **Shader 契约与跨平台收尾**：自动校验 HLSL/SPIR-V、C++ 布局和 Pipeline Layout；验证 Linux
   Clang/GCC、共享/静态构建、安装及独立 Consumer。

## H-05 退出条件与后续阶段

完成上述验证后，H-05 技术路线即视为闭环。届时暂停扩展级联阴影、自动曝光、Bloom、反射探针、
分块/聚簇和 Bindless 等新能力，除非已有性能基线明确触发对应重新评估条件。

后续工作按以下顺序推进：

1. 设计 Scene、Material、Lighting 和统一渲染入口的首版公共 C ABI，并补充轻量 C++20 RAII 包装。
2. 实现 H-07 `granit::render_pipeline`，为普通用户组合 Scene、Material、PBR、Lighting、Post Process
   和 Render Graph，同时保留直接 Renderer 与部分模块两种使用方式。
3. 完成公共 API 文档、安装导出、C/C++ Consumer 和版本/ABI 回归后，再继续新的高级渲染功能。

当前仍处于开发阶段，公共 API/ABI 可以根据验证结果调整；但一旦首版公共接口进入发布流程，内部
binding、Vulkan 对象和高频细粒度操作不得泄漏到动态库边界。

## H-05E1 实现记录

已增加独立 `granit_lighting_benchmarks` 目标和 `pack_view_point_lights` 用例，支持通过 `--lights`
选择 1、16、64 或 128 个可见点光。基准在计时区间外构建单 View Snapshot，计时内容只包含输出
容器分配、索引遍历和 GPU 光源布局打包，不混入可见性筛选、GPU 上传或 Draw。

Release + Clang 首份基线已按每组 5 次预热、30 个样本和每样本 100,000 次迭代保存。1、16、64、
128 个点光的 P50 分别约为 73、152、406、743 ns，增长近似线性且 128 光源仍低于 1 微秒。当前
没有证据需要引入并行打包、缓存或新的全局任务系统；下一步建立 GPU 上传与帧时间测量路径，再决定
是否需要分块/聚簇光照。

## H-05E2 实现记录

GPU 帧时间测量前确认现有 PBR Shader 仍只消费单方向光，不能用 CPU 已打包但 Shader 未读取的
1/16/64/128 光源生成有效 GPU 基线。因此先增加逐 View `light_buffers`：拥有 16 字节光源计数
Uniform Buffer 和方向光、点光、聚光三个 Storage Buffer，容量在初始化时固定，支持空光源集合，
并在数组写入成功后最后更新计数。

`light_buffers` 不单独创建 Bind Group，避免与阴影和 IBL 各自建立互斥的 Group 3。后续组合资源将
binding 8～11 与可选阴影、IBL binding 合并到一个 Group 3，再接入 PBR Shader 有界循环和 GPU
时间戳查询。测试覆盖空集合、容量内更新、非法上限、未初始化调用和更新溢出。

## H-05E3 实现记录

阴影与 IBL 组合资源现同时拥有 `light_buffers`，并将计数 Uniform Buffer 及三类 Storage Buffer 的
binding 8～11 合并到原有 binding 0～7 的同一个 Group 3。调用方可以更新逐 View 打包结果，不需要
绑定第二个互斥资源组；资源仍按 Bind Group、布局、光源 Buffer、Sampler 和常量 Buffer 的依赖逆序
销毁。

PBR HLSL 新增 `GRANIT_PBR_LIGHTS` 变体，按计数对方向光、点光和聚光执行有界循环，并复用 CPU
参考实现的平方反比平滑截止与聚光内外锥响应。首版约定仅第一盏方向光接收当前单级阴影，其余光源
不投射阴影。SPIR-V 反射测试固定 binding 8～11 契约；离屏整链路已改用该变体，一盏方向光的 GPU
像素继续与 CPU PBR、IBL、ACES 和 sRGB 参考一致。下一步扩展像素回归到多点光和聚光，再加入 GPU
时间戳测量。

## H-05E4 实现记录

离屏整链路像素回归已加入一盏点光和一盏聚光：点光覆盖平方反比与半径平滑截止，聚光额外覆盖
内外锥余弦响应。中心像素的 CPU 期望值由方向光、点光、聚光和 split-sum IBL 分别计算后在线性
HDR 空间相加，再经过 ACES fitted 与 Shader sRGB 编码，与最终 `RGBA8_UNORM` 回读结果比较。

遮挡用例只屏蔽约定中的第一盏方向光，点光、聚光和 IBL 贡献必须继续存在；受光用例再叠加方向光。
两条路径均通过逐通道像素比较，确认多光源循环、单方向光阴影边界及后处理组合正确。下一步实现
Renderer 时间戳查询能力，并分别测量多光源 PBR Pass 与完整后处理链的 GPU 时间。

## H-05E5a 实现记录

已建立内部 Vulkan 时间戳查询池基础层，负责创建固定容量的 Timestamp Query Pool、在命令流中
重置和写入查询、读取 64 位结果并按设备 `timestampPeriod` 统一换算为纳秒。接口校验设备、命令
句柄、查询范围和 Pipeline Stage，不向公共头文件传播 `VkQueryPool`、Vulkan Stage 或 tick 单位。

Vulkan 设备初始化现同时验证 Query Pool、命令重置/写入及结果读取函数，避免在运行到性能测量时才
发现驱动函数缺失。后端测试覆盖最小容量、重复初始化、查询越界、空命令和销毁状态。下一步将该
基础层接入 Renderer 句柄表与 Command Recorder，定义后端无关 Stage 和异步结果可用性语义，再做
真实命令提交后的时间区间回归。

## H-05E5b 实现记录

Renderer 公共层现提供后端无关的 `granit_timestamp_query_pool` 64 位整数句柄及 C ABI，C++20 层
提供 move-only RAII 包装。Command Recorder 可重置查询范围，并在 Top、全部 Graphics 或 Bottom
三个稳定阶段写入时间戳；查询结果统一返回纳秒，GPU 尚未完成时返回 `NOT_READY`，调用方也可先
通过 Recorder `reset()` 等待提交完成后读取。

查询池进入 generation/type/domain 句柄校验、Renderer 生命周期诊断和 submission serial 延迟销毁；
Recorder 录制期间会保留查询池，用户提前销毁句柄不会释放 GPU 在途对象。真实 Vulkan 回归已完成
重置、首尾写入、异步提交、等待、纳秒读取及显式销毁。测试期间还定位并修复了销毁函数参数求值
顺序问题：先保存 serial 再移动资源，避免先 `std::move` 后解引用空记录。下一步将时间戳插入 PBR
离屏链并建立分 Pass GPU 基线。

## H-05E5c 实现记录

多光源离屏整链路现使用四个时间戳，在同一次 GPU 提交中分别标记 Shadow 开始、Shadow 结束、PBR
HDR 结束和 Tone Mapping 结束。程序在 Recorder `reset()` 等待完成后读取纳秒结果，检查时间单调性，
并分别输出 Shadow、PBR HDR、Tone Mapping 和渲染链总时间；Texture 到 Readback Buffer 的复制发生
在最后一个时间戳之后，不计入渲染链。

当前示例数据仅证明分段测量正确接入，不作为性能基线。下一步将相同录制路径迁入 Release Benchmark，
按 1/16/64/128 光源执行预热、多样本采集和百分位数统计，再根据 PBR Shader GPU 时间决定是否触发
分块/聚簇光照评估。

## H-05E5d 实现记录

已增加 `granit_lighting_gpu_benchmarks`，直接复用离屏 PBR 示例的资源创建、Shader、录制和提交路径，
避免基准与实际回归形成两套渲染逻辑。基准关闭最后的像素复制与 CPU 回读，以四个 Vulkan timestamp
query 统计 Shadow、PBR HDR、Tone Mapping 和整条渲染链；每个正式样本先对多帧求均值，再计算
mean、P50、P95 和 P99。

Windows Clang Release 共享库首份基线使用 5 个预热样本、20 个正式样本、每样本 20 帧。1、16、64、
128 点光下 PBR HDR P50 分别约为 0.137、0.362、1.153、2.202 ms，近似随点光数量线性增长；Shadow
维持约 0.004 ms，Tone Mapping 维持约 0.080 ms。128 点光仍未达到本计划定义的“帧预算不可接受”
触发条件，因此当前不引入分块或聚簇光照，但线性 Forward 循环已作为后续目标硬件复测的重点。
完整环境与百分位数见 Benchmark 结果目录。H-05E 下一步进入功能降级组合验证。

## H-05E6a 实现记录

统一 Group 3 资源现通过显式 `lighting_resource_features` 区分阴影和 IBL 能力。关闭阴影时不会创建
Shadow 常量 Buffer、比较 Sampler 或 binding 0～2；关闭 IBL 时不会创建 IBL 常量 Buffer、线性
Sampler 或 binding 3～7。光源计数及三类光源 Buffer 的 binding 8～11 始终存在，因此空光源集合、
仅直接光、无阴影和无 IBL 都能使用真实匹配的稀疏布局，不需要伪造 Texture View。

初始化严格要求 Texture View 与启用能力一致：启用 IBL 必须完整提供三张视图，禁用能力时必须保持
对应视图为空；对未启用能力执行更新会返回 `INVALID_ARGUMENT`。Vulkan 回归覆盖完整组合、仅直接光、
IBL 加直接光、阴影加直接光、空光源更新、部分 IBL 资源拒绝和多余视图拒绝。下一步编译匹配这些
布局的 LIGHTS、IBL+LIGHTS、SHADOW+LIGHTS Shader 变体，并完成各组合的 GPU 像素回归。

## H-05E6b 实现记录

已从同一份 PBR HLSL 固定 `LIGHTS`、`IBL+LIGHTS` 和 `SHADOW+LIGHTS` 三种额外 SPIR-V 变体，并
保留既有 `SHADOW+IBL+LIGHTS` 完整变体。无阴影路径使用只输出 world position 的 LIGHTS Vertex
Shader；阴影路径继续使用相同接口的完整 Vertex Shader。所有新增二进制均通过 Vulkan 1.3
`spirv-val`。

Shader 反射回归分别固定三类稀疏 Group 3 契约：LIGHTS 只包含 binding 8～11，IBL+LIGHTS 包含
binding 3～11 中对应资源，SHADOW+LIGHTS 包含 binding 0～2 和 8～11。这些契约与 H-05E6a 的
可选资源布局逐项对应。下一步将四种变体分别创建真实 Pipeline，并完成六种功能组合的像素回归。

## H-05E6c 实现记录

离屏 PBR 回归现为 LIGHTS、IBL+LIGHTS、SHADOW+LIGHTS 和完整变体分别构建 Material Package、
Material Template、Pipeline 及匹配布局的 Material Instance。Granit 当前按 Bind Group Layout 句柄
身份校验绑定关系，因此结构相同的 Material Group 也不跨 Template 复用，避免依赖 Vulkan 的隐式
布局兼容推断。

同一回归连续验证零光源、仅直接光、仅 IBL、无阴影、无 IBL、完整遮挡和完整受光路径。零光源与
仅 IBL 会把真实光源计数更新为零；无阴影路径完全跳过 Shadow Pass；无 IBL 路径不创建或绑定任何
环境纹理。每种结果都由对应 CPU 参考贡献在线性 HDR 空间组合，再经过相同 ACES fitted 和 sRGB
编码与 GPU 中心像素比较，同时验证背景清屏像素。功能降级组合至此完成，下一步进入窗口与
Swapchain 闭环。

## H-05E7a 实现记录

新增 Win32 `granit_window_hdr_example`，在同一个 Frame 中先把线性 HDR 颜色写入尺寸匹配的
`RGBA16_FLOAT` 中间 Attachment，再复用 H-05D Tone Mapping Pass 输出到当前 Swapchain Backbuffer。
如果 Swapchain 为 sRGB 格式，Shader 保持线性输出并由 Attachment 编码；如果为 UNORM 格式，Shader
显式执行 sRGB 编码，继续拒绝双重编码和缺失编码。

窗口循环沿用 F-06 的 acquire、backbuffer、submit-frame、present 和瞬时 Frame 令牌模型。
Resize 或 SUBOPTIMAL/OUT_OF_DATE 会先完成当前 Frame，再重建 Swapchain，并按新尺寸和格式重建 HDR
Texture、View、Tone Mapping Pipeline 与 Bind Group；窗口客户区为零时不创建零尺寸资源，等待恢复。
`--smoke-test` 会渲染三帧并主动改变一次窗口尺寸，已通过 Validation Layer。最小化时旧 Backbuffer
保持有效以及恢复后的重建语义继续由真实 Swapchain 测试覆盖。下一步把完整 PBR/Lighting Pass 接到
同一 HDR 目标，完成离屏与窗口共用渲染链验证。

## H-05E7b 实现记录

窗口 HDR 示例现使用真实 PBR Draw 替代固定颜色清屏：从与离屏路径相同的 PBR HLSL/SPIR-V 构建
Material Package、Material Template、Pipeline、缺省纹理和 Material Instance，先输出到带 D32
Depth Attachment 的线性 HDR 目标，再执行同一个 Tone Mapping Pass 到 Swapchain。Resize 会同时
重建 HDR Color、Depth、Tone Mapping 资源，尺寸无关的 PBR 材质和 Pipeline 保持复用。

离屏与窗口示例已抽取共用的 PBR 材质包描述及材质实例初始化辅助代码，材质参数、缺省纹理和
Pipeline 状态不再维护两份。窗口路径当前使用基础单方向光 PBR Shader，用于先固定 PBR→HDR→
Tone Mapping→Present 的帧结构；离屏多光源像素回归和 GPU Benchmark 继续通过。下一步把统一
Group 3、多光源、阴影与 IBL 资源接入窗口路径，完成完整参考管线闭环。

## H-05E7c 实现记录

窗口示例现使用与离屏回归相同的 `SHADOW+IBL+LIGHTS` PBR Shader 和 Group 3 契约。示例辅助资源
创建一张真实 Shadow Depth Texture、Irradiance Cube、Prefiltered Environment Cube、BRDF LUT、
方向光/点光/聚光 Buffer 及统一 Bind Group；固定生成纹理与离屏数值回归一致，不依赖外部资产加载。

每帧按 Shadow 清屏、完整多光源 PBR Draw、HDR Tone Mapping、Swapchain Present 顺序录制。窗口
Resize 只重建尺寸相关 HDR Color/Depth 和后处理资源，光源、IBL、材质及 PBR Pipeline 保持复用；
最小化、OUT_OF_DATE、SUBOPTIMAL 和 sRGB/UNORM 传递沿用 H-05E7a 的处理。Clang Release 与 VS Debug
均通过主动 Resize 的三帧 Validation smoke test，离屏六组合像素回归和 GPU Benchmark 未发生退化。
窗口与 Swapchain 闭环至此完成，下一步进入多 View 完整渲染。

## H-05E8a 实现记录

多 View GPU 集成首先固定逐 View 隔离边界：两个 View 共享同一份 `multi_view_snapshot` 场景光源，
但使用不同 layer mask 得到各自可见索引，再分别通过 `pack_view_lights` 生成独立 GPU 布局并上传到
两个不同的 Group 3。测试明确校验两个 View 分别只获得红色和绿色点光，且 Bind Group Layout、
Bind Group 和底层光源 Buffer 不共享可变状态。

两个 View 同时拥有独立 `RGBA16_FLOAT` HDR 与 D32 Depth 目标；同一个 Command Recorder 连续录制
两组 Dynamic Rendering 后一次提交并等待完成，验证资源状态转换、强引用和批量提交边界。共享
Snapshot 在 GPU 资源初始化后不再参与录制，避免为每个 View 复制整份 Scene 数据。下一步在这两个
目标上绑定各自匹配的 Material Template/Group 3，执行真实 PBR Draw、Tone Mapping 和像素区分回归，
再将多 View 阶段标记完成。

## H-05E8b 实现记录

两个 View 现分别创建与自身 Group 3 布局句柄匹配的 Material Package、Material Template、PBR
Pipeline 和 Material Instance，并使用同一 `LIGHTS` Shader 执行真实 Draw。View 0 只上传红色点光，
View 1 只上传绿色点光；两套 PBR Pass 连续写入各自 `RGBA16_FLOAT` HDR/D32 目标后复制到独立
Readback Buffer，仍由同一个 Recorder 一次提交。

中心 HDR 半精度像素回归确认 View 0 红色分量高于绿色、View 1 绿色分量高于红色，证明 Scene
可见索引、逐 View 打包、GPU Buffer、Bind Group、Pipeline Layout 和 Attachment 没有跨 View 串用。
材质缺省资源可共享，但可变的光源和目标保持隔离。下一步为两个 HDR 目标分别创建 Tone Mapping
输出并验证显示像素，然后完成多 View 阶段。

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
