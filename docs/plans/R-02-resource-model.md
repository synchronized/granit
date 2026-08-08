<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-02：第一版资源模型

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-02
- 优先级：P0
- 前置依赖：R-01
- 后续依赖：R-03、R-04、R-05、R-06、R-07、R-09

## 背景

Granit 需要在不暴露 Vulkan 的前提下表达 Buffer、Texture、Texture View 和 Sampler。资源模型
过小会在深度附件、mipmap、Cube 和数组纹理出现后被迫推翻；一次实现全部组合又会延迟离屏清屏
与最小三角形闭环。

本任务采用“完整资源模型、最小实现范围”：先建立长期可扩展的公共概念，只承诺当前闭环真正
需要的组合。可以表达但尚未实现的组合必须明确返回 `GRANIT_ERROR_UNSUPPORTED`，不得静默降级。

## 目标

- 定义后端无关、C11 可表达且适合稳定 ABI 的资源值类型和描述结构。
- 正确区分资源存储、访问视图和采样状态。
- 让首期实现能够完成 Buffer、2D Texture、默认 Texture View 和基础 Sampler。
- 为 3D、数组、Cube、mipmap、存储资源和多重采样保留自然扩展路径。
- 明确初始数据、映射、所有权、线程安全和失败行为。

## 非目标

- R-02 只定义公共模型，不实现资源句柄和 Vulkan 对象。
- 不在本任务中实现上传命令、mipmap 生成、截图或异步回读。
- 不建立完整的设备能力查询系统。
- 不公开 Vulkan format、usage、layout、memory property 或原生句柄。
- 不承诺当前草案已经形成稳定 ABI；至少经过离屏清屏和最小三角形验证后再冻结。

## 总体决策

### 完整模型与分阶段能力

公共模型从第一版开始区分以下对象：

- **Buffer**：线性字节存储，用用途标志表达 vertex、index、uniform、storage 和 transfer。
- **Texture**：拥有图像存储、尺寸、格式、mip 层级、数组层和采样数。
- **Texture View**：引用 Texture 的格式、mip 和数组子资源范围，用于采样、存储或附件绑定。
- **Sampler**：独立描述过滤、寻址和比较行为，不拥有 Texture。

第一阶段只承诺：

- Buffer 的 vertex、index、uniform、storage、transfer source/destination 用途组合。
- 一维 Buffer 范围映射，以及 upload/readback 内存的 CPU 访问。
- 单 mip、单 layer、单 sample 的 2D Texture。
- 2D Texture 的完整范围默认 View。
- nearest/linear 过滤与 clamp/repeat 基础 Sampler。

以下能力可以在结构中表达，但初期允许返回不支持：3D、数组、Cube、多 mip、多重采样、可变格式
View、压缩格式、深度/模板 View 拆分和高级 Sampler 功能。每项能力在对应任务实现并测试后，才从
“可表达”变为“已支持”。

### 不采用的替代方案

- 不采用只有 2D Texture 且 Texture 与 View 合并的原型接口，避免后续破坏性拆分。
- 不要求用户直接选择 Vulkan 内存类型、Image Layout 或 Pipeline Stage。
- 不将“顶点数据”“贴图”等高层资产作为核心 C ABI；这类便捷能力以后放在 C++ 工具层。
- 不在首期建立完整能力查询 API；创建时执行确定性验证，后续再增加格式和限制查询。

## 公共值类型方向

所有 ABI 枚举使用 `uint32_t` 常量，不使用 C `enum` 作为结构体字段。位标志必须拒绝未知位。

### 内存位置

- `AUTOMATIC`：由 Granit 根据用途选择，默认不承诺可映射。
- `DEVICE`：面向 GPU 高频访问，公共契约禁止映射。
- `UPLOAD`：CPU 写、GPU 读，允许映射。
- `READBACK`：GPU 写、CPU 读，允许映射。

具体 Heap 和 Memory Type 始终是实现细节。UMA 设备即使使用同一种物理内存，也不能改变上述
公共行为。

### Buffer 用途

首版计划提供可组合标志：

- transfer source、transfer destination；
- vertex、index；
- uniform、storage；
- indirect 作为后续命令接口需要的预留用途。

创建时检查大小非零、标志非零、未知位、用途与内存位置冲突以及设备限制。用途标志只描述允许
的 GPU 操作，不决定 Buffer 的所有权或当前同步状态。

### Texture 维度与用途

Texture 描述包含 dimension、width、height、depth、mip_levels、array_layers、sample_count、format
和 usage。未使用的维度必须采用规范值，例如 1D 的 height/depth 为 1，2D 的 depth 为 1。

用途至少能够表达 transfer source/destination、sampled、storage、color attachment 和
depth-stencil attachment。创建时验证维度、格式、采样数和用途组合，不进行静默修正。

### 像素格式

首批格式只收录完成渲染闭环需要且语义清晰的集合：

- R8、RG8、RGBA8 的 UNORM 形式；
- RGBA8 SRGB；
- BGRA8 UNORM/SRGB，用于常见 Swapchain；
- RGBA16 float；
- D16、D32 float 和常见 depth-stencil 格式。

格式常量是 Granit 自己的稳定值，不与 `VkFormat` 数值绑定。具体支持性由后端验证；后续增加
格式只能追加常量，不能改变已有含义。

### Texture View

Texture View 使用独立 64 位句柄，并记录所属 Renderer 和父 Texture。描述包含 view dimension、
可选 format、base mip/mip count、base array layer/layer count 和 aspect。

零或专用默认值可以表示“继承 Texture 格式”与“覆盖剩余范围”，但最终编码必须保证 C API 不会
把合法零值和未初始化字段混淆。销毁 Texture 时所有子 View 的公共句柄立即失效。

普通 C++ 用户可通过便捷函数在创建 Texture 时获得默认 View，但底层仍是两个独立对象，不能
建立第二套运行时状态。

### Sampler

Sampler 描述包含 min/mag/mipmap filter、U/V/W address mode、LOD 范围、LOD bias、各向异性和
比较操作。基础实现先支持 nearest/linear、clamp/repeat；设备不支持或超过限制的配置返回明确
错误，不静默裁剪。

Sampler 使用独立句柄。内部以后可以缓存相同描述对应的 Vulkan 对象，但公共句柄的销毁与错误
语义不能依赖是否命中缓存。

## 描述结构与 ABI

- 每个创建描述以 `struct_size` 开头，并定义版本一最小尺寸常量。
- 字段只允许在结构体尾部追加；保留字段必须要求为零。
- 数量、偏移和尺寸使用明确的定宽整数；资源字节范围使用 `uint64_t`。
- 句柄使用现有 64 位整数规则，零值无效，并校验类型、generation 和 Renderer domain。
- C++20 包装只保存 C 句柄并调用 C API，提供 move-only RAII，不复制底层状态。

具体结构体与常量命名在实现前形成单独 API diff，并同时编译 C11 和 C++20 头文件测试。

## 初始数据

资源描述允许后续通过独立初始数据结构携带指针和字节数，但数据仅在创建调用期间借用。函数
返回后 Granit 不再读取调用者内存；如上传尚未完成，内部必须已经复制到自身拥有的暂存区。

初始数据不直接塞入所有资源描述，以避免创建结构持续膨胀。R-04 负责确定采用单个初始数据参数、
子资源数组还是批量上传上下文。

首期不得让普通 Buffer/Texture 创建隐式执行不可预期的同步等待。同步完成点和资源何时可被 GPU
使用，需要与 R-04 和 F 系列提交模型共同定义。

## 映射语义

- 仅 `UPLOAD` 和 `READBACK` 资源允许公开映射；`DEVICE` 即使在 UMA 上可见也返回不支持。
- 公开 API 使用 map/unmap 表达一次 CPU 访问周期，内部可以保持 VMA 持久映射。
- `UPLOAD` 在结束写访问时自动处理必要的 flush。
- `READBACK` 在 CPU 开始读取前自动处理必要的 invalidate。
- 映射范围必须进行越界检查；非一致内存对齐由后端处理。
- 初版拒绝同一资源的嵌套映射、并发写映射以及映射期间销毁。

高频动态数据以后使用 Upload Ring 或批量更新接口，避免每个小片段一次动态库调用。

## 生命周期与线程安全

- 创建成功后资源由 Renderer 拥有，句柄不可跨 Renderer 使用。
- 销毁先从 Registry 移除句柄，使调用者立即观察到失效；底层 GPU 对象可由 R-08 延迟释放。
- Texture View 不拥有父 Texture；父 Texture 失效会级联使其 View 失效。
- 不同资源允许从多个线程并行创建和销毁。
- 同一资源的 map、unmap、更新和销毁初版要求外部不得并发，库仍需拒绝可检测的非法状态。
- Registry 锁只保护身份和所有权，不在持锁时进行可能阻塞的 GPU 分配或上传。

## 错误规则

- 零尺寸、非法范围、未知标志和结构尺寸错误返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- 有效但当前实现或设备不支持的组合返回 `GRANIT_ERROR_UNSUPPORTED`。
- 错误类型、generation 或 Renderer domain 返回现有句柄错误语义。
- Host/Device 分配失败返回 `GRANIT_ERROR_OUT_OF_MEMORY`。
- 设备丢失沿用 `GRANIT_ERROR_DEVICE_LOST`。

不得因为某项能力不支持而自动更换格式、降低采样数、移除用途或改变内存位置。

## 实施顺序

1. 列出首版常量、描述结构和验证矩阵，审查 C ABI 布局。
2. 在公共 `.h` 中增加值类型，在 `.hpp` 中增加对应强类型与位运算包装。
3. 增加 C11/C++20 独立包含、结构大小、常量数值和未知位测试。
4. R-03 实现 Buffer 句柄、映射和销毁。
5. R-04 实现初始数据和 device-local Buffer 上传闭环。
6. R-05 实现 2D Texture 与默认 Texture View。
7. R-06 实现基础 Sampler，并根据真实设备验证描述模型。
8. 完成离屏清屏和最小三角形后复审 API，再决定稳定性承诺。

## 测试与验收

- 所有公共 `.h` 能由 C11 编译器独立包含，`.hpp` 能由 C++20 编译器独立包含。
- 共享库和静态库的导出宏、结构布局及常量保持一致。
- 描述验证覆盖零尺寸、未知位、格式/用途冲突、非法子资源范围和不支持组合。
- 句柄测试覆盖零值、错误类型、旧 generation、跨 Renderer 和重复销毁。
- 文档明确区分“能够表达”“当前支持”和“未来能力”。
- 公共头文件不包含 Vulkan、Volk 或 VMA 类型与头文件。
- R-02 完成时只代表资源模型落地，不把 R-03 至 R-06 的运行时能力描述为已实现。

## 后续复审点

- R-04 结合提交模型确定初始数据的同步完成语义。
- R-05 根据 Texture View 实现确定默认范围的精确编码。
- R-06 根据设备限制确定各向异性和边框颜色的首版范围。
- 首个离屏渲染闭环后评估是否需要提前加入格式/限制查询 API。
- 最小三角形完成前，公共 API 和 ABI 仍允许根据验证结果调整。

## 实现结果

- 新增独立的 `resource_types.h/.hpp`，并由聚合入口包含。
- C ABI 已定义内存位置、Buffer/Texture 用途、Texture 维度、格式、采样数、子资源范围和
  Sampler 状态；所有字段均使用定宽值类型。
- `granit_buffer_desc`、`granit_texture_desc`、`granit_texture_view_desc` 和
  `granit_sampler_desc` 均包含 `struct_size` 和固定的版本一大小。
- C++20 层提供强类型枚举和用途/aspect 位运算，不建立额外运行时状态。
- 后端无关验证已区分非法参数和合法但尚未支持的组合，首期范围保持为 Buffer、单 mip 单 layer
  2D Texture、默认 2D View 和基础 Sampler。
- 初始数据仍按计划留给 R-04；R-02 没有提前增加资源创建或销毁函数。
