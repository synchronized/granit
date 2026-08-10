<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-01：Shader 输入与离线编译策略

## 元数据

- 设计状态：已确认
- 实现状态：设计已完成
- 路线图任务：D-01
- 优先级：P0
- 前置依赖：阶段一 ABI 基础、Vulkan 1.3 后端
- 后续依赖：D-02、D-03、D-06

## 目标

确定 Shader 数据如何进入 Granit、源代码在哪里编译、反射元数据由谁生成，以及编译和运行时
错误如何报告。普通用户不接触 Vulkan Shader Module、`VkShaderStageFlagBits` 或 Vulkan 反射类型。

## 已确认决策

### 运行库只接收 SPIR-V

Granit 核心动态库第一版只接收完整 SPIR-V 二进制，不在运行时编译 GLSL、HLSL 或其他源语言：

- 公共 C ABI 使用“字节指针 + 字节数”，不要求调用者提供对齐后的 `uint32_t*`；
- Granit 在进入 Vulkan 前完成长度、Magic Number、阶段和入口点等基础校验；
- 创建成功后不依赖调用者继续保留输入内存；
- Shader 创建描述使用 `struct_size`，为后续元数据和高级选项保留尾部扩展能力；
- 公共接口不暴露 SPIR-V Tools、shaderc、DXC 或 Vulkan 类型。

运行时编译会显著增加 DLL 体积、启动延迟、依赖和错误面，并让核心库承担源语言版本、include
搜索路径、宏、变体及编译缓存策略，因此不进入基础 Renderer API。

### 离线编译属于独立工具层

后续提供可选的 `granit-shader` 命令行工具或 CMake 辅助函数，核心库不依赖该工具。第一阶段允许
项目直接使用已有编译器生成 SPIR-V，再将产物作为二进制资产交给 Granit。

离线工具应负责：

1. 源文件、include 路径、宏和目标环境的确定性输入；
2. 编译 GLSL/HLSL 为 SPIR-V，并保留完整编译器诊断；
3. 校验 SPIR-V，生成依赖清单和内容哈希；
4. 生成 Granit 自有的反射元数据，而不是把第三方反射结构写入资产格式；
5. 支持调试信息保留/剥离和可复现构建。

具体选择 glslang、DXC、shaderc 或组合工具前，必须单独记录版本、许可证、目标平台和升级策略。
D-01 不为尚未实现的工具提前引入第三方库。

### 反射只作为离线元数据来源

SPIR-V 仍是驱动使用的程序输入；资源绑定、入口点、顶点输入和 push constant 等反射结果转换为
Granit 自有、版本化的元数据。公共 API 和资产不能依赖某个反射库的枚举、结构布局或字符串
生命周期。

第一版 D-02 可以只创建 Shader 对象，D-03 再根据实际 Pipeline Layout 需求确定最小元数据格式。
在格式确认前，不把完整 SPIR-V 反射结果直接加入稳定 C ABI，也不让运行时反射成为每次启动的
强制成本。

### 入口点和阶段必须显式

Shader 对象表示“一个 SPIR-V 模块中的一个阶段入口”，创建时必须提供 Granit 自有 Shader Stage
和入口点。入口点默认值可由 C++ 包装便利地填入 `main`，但 C ABI 描述中的实际值始终明确。

第一版阶段范围：

- Vertex；
- Fragment；
- Compute。

Geometry、Tessellation、Mesh、Task 和 Ray Tracing 等阶段在对应渲染能力落地时扩展，不提前加入
未验证的 Pipeline 设计。

## 输入校验

D-02 创建 Shader 前至少检查：

- 指针非空，字节数非零且为 4 的倍数；
- 数据以 SPIR-V Magic Number 开始；
- 输入大小没有超过实现上限或发生整数溢出；
- 阶段枚举有效，入口点非空且长度在上限内；
- Renderer、Shader 和后续 Pipeline 的 domain 一致；
- Vulkan 创建失败映射为 Granit 结果码，不泄漏 `VkResult`。

完整 SPIR-V 合法性主要由离线校验工具负责。运行库只进行低成本防御性检查，不在每次加载时
重复完整编译器验证流程；Validation Layer 仍用于开发构建发现后端约束错误。

## 错误与诊断

错误分为两个边界：

- 离线编译错误：命令行工具返回非零退出码，并输出带文件、行列、阶段和 include 链的完整诊断；
- 运行时加载错误：C API 返回 Granit 结果码，输出句柄保持零值；详细日志以后通过 S-02 的统一
  诊断回调提供。

公共结果码应区分无效输入、不支持的阶段或能力、内存不足、Device Lost 和后端内部失败。不得
把编译器拥有的临时字符串指针跨 DLL 边界返回，也不得让 C++ 异常穿过 C ABI。

## CMake 与资产集成边界

- Granit 核心目标不公开传递 Shader 编译器依赖。
- 离线工具使用独立 CMake 选项和目标，关闭工具构建时不下载编译器。
- 生成规则必须声明源文件、include 和配置依赖，支持多配置生成目录。
- 安装包可分别发布运行库与工具，普通运行时部署不携带编译器 DLL。
- 测试用 SPIR-V 应由仓库可复现生成或以明确来源的最小二进制 fixture 固定，不依赖开发机全局
  Vulkan SDK 路径。

## D-02 实施顺序

1. 定义平台无关的 Shader Stage、`granit_shader` 句柄和创建描述。
2. 实现输入复制/校验及内部 Vulkan Shader Module RAII。
3. 接入 Registry domain、generation、Device Lost 和 Renderer 级联销毁。
4. 提供 move-only C++20 RAII 包装。
5. 增加 C 头编译、无效 SPIR-V、句柄生命周期和真实 Vulkan 创建测试。
6. 更新 Shader 功能文档；反射资产格式留到 D-03 的最小需求明确后确定。

## 验收标准

- 核心库不依赖源语言编译器或反射库。
- 普通用户只传 Granit 类型和 SPIR-V 字节，不包含 Vulkan 头文件。
- 输入内存在 Shader 创建返回后即可释放。
- 编译错误和运行时加载错误具有明确且不同的责任边界。
- D-02 可以在不推翻本策略的前提下直接实现 Shader 生命周期。
