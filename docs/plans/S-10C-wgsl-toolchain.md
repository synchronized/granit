<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10C：WGSL Shader 工具链

## 状态

- 实现状态：已完成；跨平台验收与公共 API 决策已记录
- 所属版本：0.4.0
- 前置依赖：S-10B
- 优先级：P2

## 背景与目标

Vulkan 主路径当前只接受 SPIR-V，`granit_shader_tool` 使用 SPIRV-Reflect 输出基础绑定信息；WebGPU
插件则暂时内置固定 WGSL。继续手工维护两份 Shader 会产生入口点、绑定布局和行为漂移，因此
S-10C 需要建立一个源码权威和可复现的双后端资产流程。

本阶段目标：

- 以 WGSL 作为新增跨后端 Shader 的源码权威。
- 使用与 Dawn 相同的锁定 Tint 修订，在构建期生成 Vulkan 1.3 SPIR-V。
- 生成后端无关、稳定排序且可版本扩展的反射清单。
- 产出同时包含 WGSL、SPIR-V 和反射信息的确定性 Shader 资产。
- 让 WebGPU 插件从资产创建 Shader，不再依赖内置固定 WGSL。
- 提供包含文件、行列、阶段和入口点的可执行诊断，并验证缓存失效规则。
- 提供独立可选的 ShaderTools SDK，让编辑器和资产构建器直接链接同一实现；CLI 只作为薄适配层。

## 非目标

- 不在 Granit 核心动态库或最终应用中链接 Tint、SPIRV-Tools 或 SPIRV-Reflect。
- 不在本阶段支持 GLSL、HLSL、运行时源码编译、热重载监听或远程 Shader 编译服务。
- 不承诺把任意现有 Vulkan SPIR-V 自动还原为可维护 WGSL。
- 不在 S-10C 增加公共后端选择、浏览器 Canvas、Emscripten 主循环或 Android 接入。
- 不以 WebGPU 自动推导 Pipeline Layout 取代 Granit 显式布局契约。

## 推荐设计

### 源码与编译器

- 新增跨后端 Shader 使用 WGSL 作为唯一可编辑源码；SPIR-V 是生成产物，不作为第二份源码维护。
- Tint 使用 ADR-004 锁定的 Dawn 修订构建，并作为开发 SDK 中的私有命令行工具提供。Granit
  工具通过受控子进程调用 Tint，不链接其不稳定 C++ API。
- Tint 固定输出 Vulkan 1.3 SPIR-V，并按入口点分别生成，避免未使用入口和资源影响反射及缓存。
- 原始 WGSL 以 UTF-8、LF 和无 BOM 保存；工具不做会改变语义或诊断行号的自动格式化。

### 反射权威

- 反射清单从最终 SPIR-V 使用 SPIRV-Reflect 生成，因此描述的是 Vulkan 实际消费的产物。
- Tint 对 WGSL 的解析、验证和绑定检查作为前置闸门；测试阶段比较 WGSL 与 SPIR-V 的入口点和
  绑定集合，发现转换差异时拒绝资产。
- 清单首版包含：Schema 版本、阶段、入口点、Group/Binding、资源类型、访问模式、数组长度、
  Buffer 最小尺寸、Vertex 输入、Fragment 输出、Compute Workgroup 大小和 Override 常量。
- 所有集合按数字键稳定排序；未知资源类型必须报错，不输出含 `unsupported` 的可用资产。

### 资产与缓存

- 每个阶段入口生成一个版本化 `.granit-shader` 二进制资产，包含固定头、WGSL、SPIR-V、反射表和
  字符串表；各区段使用显式偏移、长度和对齐，不直接序列化 C++ 对象。
- 资产使用 SHA-256 内容摘要；实现复用现有材质归档的 SHA-256 逻辑并提取为内部通用工具，不新增
  第三方哈希依赖。
- 缓存键由规范化 WGSL 字节、入口点、阶段、Tint 修订、目标环境、编译选项和资产 Schema 版本
  共同计算。任一输入变化都必须失效，输出目录和绝对路径不得进入缓存键。
- 首版使用调用方指定的文件缓存目录和原子替换；并发写入同一键时允许重复编译，但不得产生半个
  资产或读取未完成文件。

### 运行时边界

- S-10C 先扩展实验性插件 ABI，增加 Shader 句柄、WGSL Shader 创建/销毁和引用 Shader 的
  Render Pipeline 描述；插件 ABI 尚未发布，因此直接演进当前版本，不保留旧操作表分支。
- WebGPU 插件复制资产中的 WGSL 后创建 `WGPUShaderModule`，异步获取编译信息并通过 Host 诊断
  回调报告；Pipeline 不再创建或销毁临时固定 Shader。
- Vulkan 公共路径继续消费资产中的 SPIR-V，现有 `granit_shader_desc` 在 S-10C 内不立即破坏。
  是否增加公共 Shader 资产创建 API，留到插件闭环验证后单独审议，避免提前锁定 0.4.0 ABI。
- 反射清单用于校验显式 Bind Group Layout、Pipeline Layout 和阶段可见性，不在运行时自动修改布局。

### 工具 SDK 边界

- 新增独立可选 `ShaderTools` 安装组件和 `granit::shader_tools` CMake 目标，不成为
  `granit::granit` 的传递依赖。
- `.h` 提供 C11 ABI，使用版本化描述、整数句柄、调用期字符串视图和成对结果销毁函数；`.hpp`
  在其上提供 C++20 RAII、容器和字符串便利接口。
- SDK 返回结构化诊断与反射记录，不暴露 Tint、SPIRV-Reflect、STL、异常或平台进程句柄。
- `granit_shader_tool` 的 `main()` 只解析参数、调用 SDK、格式化输出和映射退出码，不复制编译、
  校验、反射或缓存逻辑。
- 项目仍处于早期，旧的单参数 CLI 入口直接删除，不建立弃用兼容分支。

## 实施顺序

1. **S-10C1 编译基线**：已让锁定 Dawn SDK 同时产出 Tint CLI；最小 WGSL Fixture 的 Vertex 与
   Fragment 入口已在 Windows/Linux 完成校验并生成有效 Vulkan SPIR-V。
2. **S-10C2 工具入口与 SDK 边界**：已增加 `compile`、`inspect` 和 `verify` 子命令及不经过 Shell
   的跨平台 Tint 子进程；旧单参数入口已删除。ShaderTools SDK 已提供首版 C ABI、C++20 RAII
   包装、安装导出和结果生命周期测试，CLI 已改为 SDK 的薄适配层。结构化反射记录继续由
   S-10C3 补齐。
3. **S-10C3 反射 Schema**：已提供稳定 JSON，以及按数字键排序的描述符、Vertex 输入和 Fragment
   输出记录；字段覆盖资源类型、访问模式、数组数量、Buffer 最小尺寸、接口标量类型/位宽/向量
   宽度、Compute Workgroup 和 Override 常量。SDK 已支持调用方传入 WGSL 预期 Binding 集合并与
   最终 SPIR-V 严格比较；继续基于锁定 Tint 的可验证机器输出自动生成预期集合，不引入脆弱的
   WGSL 源码正则解析。
4. **S-10C4 确定性资产与缓存**：已完成私有 `.granit-shader` 容器的确定性编解码、SHA-256、
   Magic/Schema/布局/损坏检测、同内容缓存命中和跨平台原子替换。ShaderTools SDK 和 CLI 已能将
   WGSL、SPIR-V 与稳定反射 JSON 写入同一资产；缓存键覆盖源码、入口点、阶段、Tint 修订、目标
   环境和编译选项，并已验证路径无关、重复构建一致及各输入变化均失效。
5. **S-10C5 WebGPU Shader ABI**：已增加实验性插件 Shader 资源和显式 Pipeline 描述，WebGPU
   插件不再内置固定 WGSL。Mock 回归已覆盖归属、跨实例混用、重复销毁、依赖顺序和实例级联清理；
   Windows D3D12 与 Linux Vulkan/Lavapipe 已通过真实 Dawn Shader/Pipeline smoke test。
6. **S-10C6 双后端 Fixture**：已让同一组 `.granit-shader` 的 WGSL 在 WebGPU、SPIR-V 在 Vulkan
   上绘制相同离屏三角形，并使用统一量化容差比较中心与角落像素。Windows D3D12 的 WebGPU
   路径以及 Linux Vulkan/Lavapipe 的 WebGPU、Vulkan 双后端运行验证均已通过。
7. **S-10C7 跨平台验收**：已手动运行 Windows D3D12、Linux Vulkan/Lavapipe 和普通 Vulkan
   回归。首次编译分别为 101 ms 和 6 ms，缓存恢复分别为 6 ms 和 3 ms；Vertex/Fragment 资产为
   1739/1296 bytes。真实 Tint 诊断包含文件、行列、入口点和阶段，失败不留下输出文件。验收结果及
   暂不增加核心公共 Shader 资产 API 的决定见
   [S-10C 验收记录](../records/2026-08-27-s10c-wgsl-toolchain-acceptance.md)。

## 测试与验收

- 合法 WGSL 能稳定生成可由 `spirv-val --target-env vulkan1.3` 接受的 SPIR-V。
- 语法、类型、入口点、阶段和绑定错误包含源文件、行列及原始 Tint 信息，失败时不留下有效资产。
- 相同输入和工具版本生成逐字节相同资产；修改源码、入口点、选项或 Tint 修订会改变缓存键。
- 资产解析覆盖错误 Magic、未知 Schema、越界区段、错误对齐、重复记录和摘要不匹配。
- WebGPU Shader/Pipeline 与 Vulkan Shader/Pipeline 均遵守句柄归属、依赖销毁和实例级联清理。
- 同一 Fixture 在 Windows WebGPU、Linux WebGPU 和 Vulkan 上通过确定性离屏像素回归。
- Granit 公共头、核心链接接口和安装 Consumer 不传播 Tint、Dawn、SPIRV-Tools 或 SPIRV-Reflect。

## 风险与待确认问题

- **公共 API 时机**：S-10C 已决定暂不新增核心资产创建函数；ShaderTools SDK 保持独立，待
  WebGPU 进入公共 Renderer 抽象或 S-10D/S-10E 明确资源加载契约后再审议。
- **Tint CLI 稳定性**：命令行参数不是 Granit 公共契约；升级 Dawn 时必须通过锁定 Fixture 验证，
  不允许静默使用系统中任意版本的 Tint。
- **反射覆盖**：SPIRV-Reflect 对 WGSL Override、纹理细分类和访问模式的表达可能不足；缺失字段
  应由 Tint 检查补齐或明确限制，而不是猜测默认值。
- **坐标语义**：WGSL 转 SPIR-V 时的 Y 方向、深度范围和组合 Sampler 差异必须由固定像素 Fixture
  验证，不能只比较反射文本。
- **资产职责**：`.granit-shader` 是构建产物格式，不在 0.4.0 自动升级为长期稳定文件格式；Schema
  不兼容时应明确拒绝并要求重新编译。
