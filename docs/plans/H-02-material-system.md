<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-02：材质参数、Shader 变体与离线构建

## 元数据

- 设计状态：已确认
- 实现状态：进行中（H-02A～H-02D、H-02E1～H-02E2 已完成）
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

## 离线编译与反射工具链

H-02D 已确定首版以 HLSL 为源语言，使用外部 DXC 命令行生成 Vulkan 1.3 SPIR-V，再用
`spirv-val`（可用时）校验，并通过 SPIRV-Reflect 转换成 Granit 自有元数据。验证范围包括：

- 目标语言与 Vulkan 1.3 SPIR-V 支持。
- include、宏、优化、调试信息和可复现构建能力。
- 反射输出能否稳定转换成 Granit 自有元数据。
- 库体积、命令行部署、许可证、版本锁定和跨平台维护成本。

DXC 不链接进运行时或核心构建；SPIRV-Reflect 和 SPIRV-Headers 也只进入离线工具目标。运行库
只消费编译后的 SPIR-V 和 Granit 元数据。工具链参考版本为 DXC `v1.8.2505.1`；仓库内锁定
SPIRV-Reflect 与 SPIRV-Headers `vulkan-sdk-1.4.350.0`。版本升级必须重新运行固定 Shader 的编译、
校验和反射测试；DXC 版本及编译参数写入产物属于 H-02E。

## 实施顺序

1. **H-02A（已完成）**：确认模板、实例、参数、绑定频率、变体和离线边界。
2. **H-02B（已完成）**：已实现纯 CPU 材质元数据、稳定参数 ID、布局校验和实例 shadow
   buffer。
3. **H-02C（已完成）**：已接入材质 Uniform Buffer、Texture/Sampler Bind Group 和 dirty 区间
   批量上传。
4. **H-02D（已完成）**：已用 DXC、spirv-val 和 SPIRV-Reflect 完成固定 HLSL 的端到端原型，
   并确定参考版本、内置依赖版本和许可证。
5. **H-02E（已完成）**：H-02E1 已完成内存版本化材质包和稳定变体查找，H-02E2 已接入 Shader、
   Pipeline Layout 和 Graphics Pipeline 缓存；H-02E3 的持久化包格式与调试导出见
   [独立计划](H-02-material-package-format.md)。按 D-09A 显式记录绑定模型和 Renderer 能力要求，
   但首版只接受传统 Bind Group。
6. **H-02F（已完成）**：已实现事务式参数/GPU 实例迁移、热替换槽、错误材质 Pipeline 回退、
   可选材质模块目标和端到端示例。
7. **H-02G**：建立参数更新、变体查找和 Pipeline 命中率性能基线。

## 首版不做

- 不在运行时解析或编译 GLSL/HLSL。
- 不提供可编程材质节点图或 Shader Graph。
- 不自动推导 PBR 参数、渲染队列或透明排序。
- H-02 首版不实现 Bindless；其 Renderer 能力边界和后续材质接入由 D-09 负责。
- 不实现 Descriptor Buffer、Ray Tracing 或 Mesh Shader 材质路径。
- 不把材质、Scene 或资产文件类型加入核心 Renderer C ABI。

## H-02B 实现记录

内部 `material_metadata` 原型使用固定 FNV-1a 64 对参数名称的 UTF-8 字节生成稳定 ID，加载时校验
已有 ID 并拒绝名称重复或哈希碰撞。常量参数元数据显式保存类型、offset、数组长度、stride 和
可选默认值；构建阶段拒绝未对齐、越界、范围重叠及默认值尺寸不匹配。

`material_instance_data` 根据模板创建 CPU shadow buffer，应用默认值并把初始常量块整体标记为
dirty。类型安全的参数写入通过 ID 查找，拒绝不存在、资源类型、类型不匹配和尺寸不匹配；相同
字节不重复标脏，多次修改合并为一个覆盖范围，供 H-02C 单次批量上传。

该实现现由开发中的 `granit::material` 静态模块构建，不进入核心动态库或安装导出。Texture View
和 Sampler 作为元数据资源类型，由 H-02C 的 GPU 实例完成实际绑定。

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

该实现位于开发中的 `granit::material` 静态模块，不进入核心动态库或安装导出。材质实例借用资源
句柄，不拥有 Texture View、Sampler、Bind Group Layout 或元数据；调用方必须保证它们覆盖实例
生命周期。

## H-02D 实现记录

可选目标 `granit_shader_tool` 由 `GRANIT_BUILD_TOOLS=ON` 启用，读取 SPIR-V 并以稳定顺序输出入口、
Shader 阶段及描述符组、binding、类型、名称和尺寸。原型 fixture 使用 HLSL 显式指定 Group 1 的
Uniform Buffer、Texture 和 Sampler，避免依赖编译器的隐式寄存器分配。

端到端测试通过外部 DXC 编译 fixture，以 `spirv-val --target-env vulkan1.3` 校验产物，并检查反射
结果与材质绑定约定一致。工具当前只覆盖 vertex、fragment、compute 以及材质原型需要的描述符
类型；尚未定义持久化包格式、include 图、变体枚举或完整诊断协议，这些属于 H-02E。

## H-02E1 实现记录

内部 `material_package` 原型保存格式版本、目标环境、绑定模型、参数元数据和按 Pass 组织的 Shader
变体。当前只接受格式版本 1、Vulkan 1.3 与传统 Bind Group；Bindless 枚举值用于让不支持的包被
明确拒绝，不代表已经实现 D-09。

构建阶段按 feature ID 排序并拒绝零 ID、重复特性、重复变体、缺少 Vertex/Fragment 阶段、重复
Shader 阶段及无效 SPIR-V magic。变体键使用固定 FNV-1a 64、显式小端字节顺序及特性数量计算，
不依赖 `std::hash` 或输入排列。包按“Pass ID + 变体键”排序并进行无分配查找。

该原型仍是内存结构，不是磁盘格式，也不进入公共 ABI 或安装导出。magic、区段表、长度校验、
内容哈希、编译器身份和编译参数将在 H-02E3 持久化格式落地时补充。

## H-02E2 实现记录

内部 `material_template_gpu` 从包的参数元数据创建材质 Bind Group Layout，并创建空的 Group 0 与
材质 Group 1 组成的 Pipeline Layout。这样 Shader 可以遵循既定的 Group 1 材质约定，同时不要求
H-02 提前实现 frame/view 资源。

首次请求“Pass ID + 变体键 + 颜色格式 + 深度格式 + 采样数”时创建包内 Vertex/Fragment Shader
与 Graphics Pipeline，后续相同请求复用同一句柄。缓存互斥覆盖查找和首次创建，因此同一键的并发
请求只产生一个缓存项。失败路径会逆序销毁已创建对象，不发布半初始化条目；模板销毁时按 Pipeline、
Shader、Pipeline Layout、Bind Group Layout 顺序释放。

当前只支持单颜色 Attachment 和默认图元、深度及混合状态。顶点布局、多个颜色 Attachment 和
固定 Pipeline 状态必须成为包内显式数据后才能扩展缓存键，不能通过隐藏默认值冒充完整材质格式。

## H-02F1 实现记录

热替换不原地修改旧模板或实例。`migrate_material_instance_data` 先依据新元数据创建独立实例，再按
稳定参数 ID 迁移常量值；只有全部处理成功才发布新实例和迁移报告，失败时调用方原有输出保持不变。

类型和数组数量必须匹配，数组 stride 可以变化并按元素重新布局。新参数、类型变化或数组数量变化
保留新模板默认值，并在报告中给出参数 ID 和原因。资源参数暂记为待处理，不复制裸句柄；资源所属
Renderer 校验、替代 Bind Group 创建和 GPU 对象切换属于 H-02F2。

## H-02F2 实现记录

`material_gpu_instance::prepare_migration` 在独立候选实例中创建新 Uniform Buffer，复用 H-02F1
结果，并仅复制稳定 ID 与资源类型均匹配且已经设置的 Texture View/Sampler 句柄。缺失、类型变化
或旧实例尚未设置的资源保持为空并写入迁移报告，调用方可在候选实例上补齐。

候选实例调用 `flush` 时重新创建 Bind Group，因此复用的句柄仍会经过现有 Renderer/domain 和
资源类型校验。成功后调用无失败的 `swap` 发布候选；旧 Buffer 与 Bind Group 被转移到替代壳中，
由现有延迟销毁路径释放。准备、分配或 Bind Group 创建失败均不修改正在使用的旧实例。

## H-02F3 实现记录

`material_runtime_template` 共同拥有不可变材质包和借用它的 GPU 模板，析构顺序保证 Pipeline、
Shader 与 Layout 先于包释放。`material_hot_reload_slot` 在锁外构建候选，成功后才在短临界区内替换
活动模板；快照使用 `shared_ptr`，因此旧模板和在途 Recorder 可以自然延长生命周期。

首次加载失败且没有活动模板时返回调用方提供的 fallback；后续重载失败保留上一代活动模板且不
增加 generation。Pipeline 获取先尝试活动材质，缺少变体或底层创建失败时再尝试 fallback，并在
结果中同时返回原始错误、是否回退以及模板 keepalive。Renderer 不内置 Shader 或规定错误材质的
外观，上层应提供并预先验证一个醒目的错误材质包。

## H-02F4 实现记录

运行时材质源码统一编译为 `granit_material` 静态目标，并提供仓库内别名 `granit::material`；测试、
材质工具和示例不再各自重复编译实现。模块公开依赖 `granit::granit`，但当前 API/ABI 尚未稳定，
因此暂不加入安装导出。源 JSON 解析和调试 JSON 导出仍只编译进离线工具。

`granit_material_hot_reload_example` 使用预编译 SPIR-V 创建调用方错误材质，先验证活动包缺少
`opaque` 变体时取得 fallback Pipeline，再发布包含该变体的新包并验证 generation 增加且不再
回退。示例只使用 Granit 类型，不包含 Vulkan 头文件。

## 验收标准

- 通用材质层不包含 Vulkan 类型，也不改变底层显式 API。
- 参数布局、变体 key 和离线产物可复现，并能拒绝碰撞或损坏输入。
- 数值参数更新不重建 Pipeline/Layout，资源更新不触发 Shader 编译。
- 相同 Pipeline 键并发请求只创建一个底层 Pipeline。
- 热替换失败保留旧模板，成功替换不破坏在途 Recorder。
- C++ 原型稳定并完成端到端验证后，再单独决定是否提供 C ABI 或仅作为可选 C++ 高层模块。
