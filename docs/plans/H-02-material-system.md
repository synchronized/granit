<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-02：材质参数、Shader 变体与离线构建

## 元数据

- 设计状态：已确认
- 实现状态：进行中（H-02A～H-02C 已完成）
- 路线图任务：H-02
- 优先级：P2
- 前置依赖：D-01～D-08、H-01
- 后续依赖：H-03、H-04、H-05

## 目标

H-02 在核心 Renderer 之上提供可选材质模块，负责把命名参数、Texture/Sampler 绑定、固定渲染
状态和预编译 Shader 变体组织为可复用对象。普通材质使用者不直接创建 Pipeline Layout、Bind
Group 或 Pipeline，但仍可完全绕过材质模块使用底层显式 API。

材质系统不负责 Scene、Entity、网格资产、灯光、可见性、文件监控或源 Shader 编辑器，也不把
PBR 特有字段固化到通用材质接口。

## 分层模型

### 材质模板

材质模板是不可变定义，包含：

- 参数和资源槽的稳定 ID、名称、类型、数组长度、默认值及显式字节布局。
- 支持的 Pass，例如 depth、shadow、opaque、transparent 或使用者自定义 Pass ID。
- 每个 Pass 可用的预编译 Shader 变体和对应 Pipeline 固定状态。
- Bind Group Layout、Pipeline Layout 及按目标格式创建 Pipeline 所需的信息。
- 离线包版本、内容哈希和诊断名称。

模板不保存每个物体的变换、骨骼数据或动态灯光列表。模板热替换通过创建新对象并由上层原子替换
引用完成，不原地修改正在被 Recorder 使用的对象。

### 材质实例

材质实例引用一个模板，并保存：

- 参数常量块的实例值。
- Texture View 和 Sampler 等材质级资源绑定。
- 当前启用的静态特性值，用于选择模板已经包含的变体。
- 参数或资源变更后的 dirty 区间与 Bind Group 重建状态。

实例不拥有导入的 Texture View 或 Sampler；它们必须与 Renderer 属于同一 domain，并在实例使用
期间有效。实例内部生成的 Uniform Buffer 和 Bind Group 由实例拥有，并按 Recorder/GPU 完成点
安全退役。

### 离线材质包

离线工具接收材质描述、Shader 源、include、宏和目标环境，输出版本化材质包。运行库只加载包内
已经编译和验证的 SPIR-V 与 Granit 自有元数据，不依赖 DXC、glslang、shaderc 或反射库。

首个原型先使用内存中的材质包结构和固定测试数据验证布局，不立即冻结磁盘二进制格式。正式文件
格式必须具备 magic、格式版本、目标环境、内容哈希、长度校验和分区表，不能直接序列化 C/C++
结构体、指针、STL 对象或第三方反射类型。

## 参数模型

第一版通用参数类型建议限制为：

- `bool32`、`int32`、`uint32`、`float32`。
- 2/3/4 分量 float vector。
- 4×4 float matrix。
- Texture View 与 Sampler 资源槽。

运行时按稳定参数 ID 查询，名称只用于构建、编辑器和诊断。ID 由离线工具使用明确、版本化的哈希
算法生成，必须检测碰撞；不能使用实现相关的 `std::hash`。参数 ID、类型、offset、size 和数组
stride 均由离线反射元数据明确给出，运行时不重新推导 GLSL/HLSL 布局。

常量参数写入材质 Uniform Buffer。频繁逐字段修改先写 CPU shadow buffer，并合并 dirty 区间后
批量上传；不能让每次 setter 都创建 Buffer 或单独提交 GPU 工作。Texture/Sampler 变化才重建
对应 Bind Group，数值参数变化不应重建 Layout 或 Pipeline。

## 绑定频率约定

内置高层渲染模块使用以下约定：

| Group | 频率 | 典型内容 |
| ---: | --- | --- |
| 0 | frame/view | Camera、时间、全局环境与灯光索引 |
| 1 | material | 材质常量、Texture 与 Sampler |
| 2 | object | 变换、蒙皮和对象索引 |
| 3 | pass | 阴影、后处理输入及临时 Pass 数据 |

Group 4～7 暂不分配。该约定服务 Granit 自带 H-03～H-05 模块；通用底层 Pipeline API 继续允许
使用者自定义 Layout。离线工具必须校验同一模板所有变体的材质级布局兼容，不能在变体切换时悄然
改变 Group 1 的绑定含义。

## Shader 变体

变体只表示会改变 Shader 代码、Pipeline Layout 或固定 Pipeline 状态的静态特性，例如 alpha
模式、双面、法线贴图、蒙皮或顶点属性组合。颜色、粗糙度、Texture 句柄等普通运行时值不能成为
变体，以避免组合爆炸。

- 离线描述显式列出合法特性、取值和需要构建的组合。
- 离线工具对规范化的“特性 ID + 值”序列计算稳定 64 位 key，并检测碰撞。
- 材质包只包含实际请求或规则生成的合法组合，不生成所有理论笛卡尔积。
- 运行时只执行 key 查找和 Pipeline 获取，禁止回退到源代码编译。
- 缺失变体返回明确错误；开发模式可以使用显眼的错误材质，但不能静默选错 Pipeline。

变体 key 只是材质包内部身份，不作为跨版本持久 API。修改哈希算法、特性定义或 Shader 编译选项
必须改变材质包内容哈希并触发重新构建。

## Pipeline 与缓存

材质模板拥有“变体描述到 Granit Pipeline”的缓存，但 Pipeline 仍由现有 Renderer API 创建。
Graphics Pipeline 的最终键至少还取决于颜色/深度格式、采样数、顶点布局和固定状态，因此不能只
用变体 key 作为完整 Pipeline key。

首版同步按需创建 Pipeline，并复用 D-08 的 Vulkan Pipeline Cache。离线包不存放可跨设备复用的
Vulkan Pipeline Cache 数据。并发请求同一键时必须合并为一次创建；异步创建继续由使用者现有
Job System 调度，不在材质模块创建线程池。

## 错误与热重载

- 包格式、布局、变体和资源类型错误在创建模板时一次性报告。
- 设置不存在的参数、类型不匹配或跨 Renderer 资源返回明确结果，不做隐式数值转换。
- Shader、Layout、Pipeline 和材质模板均保持不可变。
- 热重载成功后新实例使用新模板；旧实例和在途 Recorder 继续持有旧对象直到安全完成。
- 自动迁移只能复制稳定 ID 与类型均匹配的参数；不匹配项恢复新模板默认值并产生诊断。
- 文件监视、重新编译和替换时机由 Asset/工具层负责，核心 Renderer 不读取材质文件。

## 离线编译器选择边界

H-02A 不立即锁定 DXC、glslang 或 shaderc。选择前必须用同一组 Shader 验证：

- 目标语言与 Vulkan 1.3 SPIR-V 支持。
- include、宏、优化、调试信息和可复现构建能力。
- 反射输出能否稳定转换成 Granit 自有元数据。
- 库体积、命令行部署、许可证、版本锁定和跨平台维护成本。

优先采用独立命令行进程而不是把编译器库链接进运行时或核心构建。具体选择在 H-02D 原型数据后
单独记录。

## 实施顺序

1. **H-02A（已完成）**：确认模板、实例、参数、绑定频率、变体和离线边界。
2. **H-02B（已完成）**：已实现纯 CPU 材质元数据、稳定参数 ID、布局校验和实例 shadow
   buffer。
3. **H-02C（已完成）**：已接入材质 Uniform Buffer、Texture/Sampler Bind Group 和 dirty 区间
   批量上传。
4. **H-02D**：原型验证离线 Shader 编译与反射工具，确定编译器、版本和许可证。
5. **H-02E**：定义版本化材质包，接入变体查找和 Pipeline 缓存。
6. **H-02F**：增加热替换、实例迁移、错误材质和端到端示例。
7. **H-02G**：建立参数更新、变体查找和 Pipeline 命中率性能基线。

## 首版不做

- 不在运行时解析或编译 GLSL/HLSL。
- 不提供可编程材质节点图或 Shader Graph。
- 不自动推导 PBR 参数、渲染队列或透明排序。
- 不实现 Bindless、Descriptor Buffer、Ray Tracing 或 Mesh Shader 材质路径。
- 不把材质、Scene 或资产文件类型加入核心 Renderer C ABI。

## H-02B 实现记录

内部 `material_metadata` 原型使用固定 FNV-1a 64 对参数名称的 UTF-8 字节生成稳定 ID，加载时校验
已有 ID 并拒绝名称重复或哈希碰撞。常量参数元数据显式保存类型、offset、数组长度、stride 和
可选默认值；构建阶段拒绝未对齐、越界、范围重叠及默认值尺寸不匹配。

`material_instance_data` 根据模板创建 CPU shadow buffer，应用默认值并把初始常量块整体标记为
dirty。类型安全的参数写入通过 ID 查找，拒绝不存在、资源类型、类型不匹配和尺寸不匹配；相同
字节不重复标脏，多次修改合并为一个覆盖范围，供 H-02C 单次批量上传。

该原型只由独立 Catch2 测试目标构建，不进入核心动态库或安装导出。Texture View 和 Sampler
作为元数据资源类型，由 H-02C 的 GPU 实例完成实际绑定。

## H-02C 实现记录

内部 `material_gpu_instance` 引用仍须比实例长寿的元数据，并拥有一个 UPLOAD Uniform Buffer 和
当前材质 Bind Group。binding 0 固定为存在常量块时的 Uniform Buffer；Texture View 与 Sampler
参数必须使用唯一且非零的显式 binding。元数据构建阶段会拒绝资源 binding 冲突。

`flush` 先把合并后的 dirty 区间通过一次 `granit_buffer_write` 上传；资源尚未全部绑定时返回
`GRANIT_ERROR_NOT_READY`，但已经成功的常量上传不会重复执行。资源未变化时后续数值更新不会重建
Bind Group。

Texture View 或 Sampler 变化后，实例使用现有不可变 Bind Group API 创建完整替代对象；只有新
对象创建成功才替换并销毁旧 Bind Group。创建失败时旧对象保持有效，资源 dirty 状态保留以便
重试。测试覆盖真实 Vulkan Buffer、Texture、Sampler、Bind Group 创建、替换和逆序销毁。

该实现仍只由独立测试目标构建，不进入核心动态库或安装导出。材质实例借用资源句柄，不拥有
Texture View、Sampler、Bind Group Layout 或元数据；调用方必须保证它们覆盖实例生命周期。

## 验收标准

- 通用材质层不包含 Vulkan 类型，也不改变底层显式 API。
- 参数布局、变体 key 和离线产物可复现，并能拒绝碰撞或损坏输入。
- 数值参数更新不重建 Pipeline/Layout，资源更新不触发 Shader 编译。
- 相同 Pipeline 键并发请求只创建一个底层 Pipeline。
- 热替换失败保留旧模板，成功替换不破坏在途 Recorder。
- C++ 原型稳定并完成端到端验证后，再单独决定是否提供 C ABI 或仅作为可选 C++ 高层模块。
