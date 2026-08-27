<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10C：WGSL Shader 工具链

## 状态

- 实现状态：实现中；推荐设计已确认，当前阶段为 S-10C1 编译基线
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

## 实施顺序

1. **S-10C1 编译基线**：让锁定 Dawn SDK 同时产出 Tint CLI；固定一个最小 WGSL Fixture，验证
   Windows/Linux 上 WGSL 校验及 Vulkan 1.3 SPIR-V 输出一致。
2. **S-10C2 工具入口**：将 `granit_shader_tool` 扩展为 `compile`、`inspect` 和 `verify` 子命令，
   保留现有单参数反射入口的迁移提示；捕获 Tint 退出码、标准输出和标准错误。
3. **S-10C3 反射 Schema**：扩展 SPIRV-Reflect 提取范围，定义后端无关记录及稳定 JSON 调试输出，
   补充 WGSL/SPIR-V 入口点和绑定集合一致性检查。
4. **S-10C4 确定性资产与缓存**：实现 `.granit-shader` 编解码、SHA-256、损坏检测、原子写入和缓存
   命中；相同输入在不同目录及两次构建中必须逐字节一致。
5. **S-10C5 WebGPU Shader ABI**：增加插件 Shader 资源和 Pipeline 描述，替换内置 WGSL，覆盖
   归属、重复销毁、依赖顺序、错误诊断和实例级联清理。
6. **S-10C6 双后端 Fixture**：同一 `.granit-shader` 的 WGSL 在 WebGPU、SPIR-V 在 Vulkan 上绘制
   相同离屏三角形，并比较允许量化容差后的关键像素。
7. **S-10C7 跨平台验收**：手动运行 Windows D3D12、Linux Vulkan/Lavapipe 和普通 Vulkan 回归，
   记录编译时间、缓存命中时间、资产大小及诊断示例，再决定公共 Shader 资产 API。

## 测试与验收

- 合法 WGSL 能稳定生成可由 `spirv-val --target-env vulkan1.3` 接受的 SPIR-V。
- 语法、类型、入口点、阶段和绑定错误包含源文件、行列及原始 Tint 信息，失败时不留下有效资产。
- 相同输入和工具版本生成逐字节相同资产；修改源码、入口点、选项或 Tint 修订会改变缓存键。
- 资产解析覆盖错误 Magic、未知 Schema、越界区段、错误对齐、重复记录和摘要不匹配。
- WebGPU Shader/Pipeline 与 Vulkan Shader/Pipeline 均遵守句柄归属、依赖销毁和实例级联清理。
- 同一 Fixture 在 Windows WebGPU、Linux WebGPU 和 Vulkan 上通过确定性离屏像素回归。
- Granit 公共头、核心链接接口和安装 Consumer 不传播 Tint、Dawn、SPIRV-Tools 或 SPIRV-Reflect。

## 风险与待确认问题

- **公共 API 时机**：推荐先完成插件和资产闭环，再决定新增资产创建函数，避免直接扩展现有
  SPIR-V 描述后形成两套互斥字段。
- **Tint CLI 稳定性**：命令行参数不是 Granit 公共契约；升级 Dawn 时必须通过锁定 Fixture 验证，
  不允许静默使用系统中任意版本的 Tint。
- **反射覆盖**：SPIRV-Reflect 对 WGSL Override、纹理细分类和访问模式的表达可能不足；缺失字段
  应由 Tint 检查补齐或明确限制，而不是猜测默认值。
- **坐标语义**：WGSL 转 SPIR-V 时的 Y 方向、深度范围和组合 Sampler 差异必须由固定像素 Fixture
  验证，不能只比较反射文本。
- **资产职责**：`.granit-shader` 是构建产物格式，不在 0.4.0 自动升级为长期稳定文件格式；Schema
  不兼容时应明确拒绝并要求重新编译。
