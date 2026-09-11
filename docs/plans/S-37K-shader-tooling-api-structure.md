<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-37K：Shader 工具接口与组织收敛

## 状态

**本地完成。** S-37K1 已完成共享 Shader 类型收敛；S-37K2 已完成统一 Compiler 配置、编译描述和
单一编译入口；S-37K2A 已完成通用内容摘要收敛；S-37K3 已完成 Compilation 与 Reflection
边界；S-37K4 已完成 Shader Object 边界；S-37K5 已完成 Library Builder 与 CLI 拆分；S-37K6
已完成资产、安装与本地发布验收。

## 背景与目标

本计划开始时，Shader 工具能力已经覆盖 WGSL、HLSL、SPIR-V 反射、缓存、单 Shader 中间产物和
Library 链接，但接口与源码组织仍保留多轮演进痕迹：

- `shader_tools.h/.hpp` 同时公开编译、反射、缓存、资产写入、工具身份和目标能力查询。
- WGSL 与 HLSL 使用重复描述和两个编译入口，调用方需要理解各前端的内部转换路径。
- `shader_tools::result` 同时表示操作结果、编译产物、反射视图和资产写入器，名称和职责均不明确。
- `src/shader_format`、`tools/shader_asset.*` 和 `shader_tools_core.*` 已分别承担格式、文件和
  编译职责，但工具侧的其余源码仍未按这一边界完成拆分。
- `shader_tool_main.cpp` 同时实现参数解析、编译编排、缓存恢复、对象写入、Library 链接和输出格式化。
- Renderer 与 ShaderTools 重复定义 stage、code format、backend 和固定长度摘要等公共概念。

本任务完成后，源码到运行时 Shader 的数据流固定为：

```text
WGSL / HLSL
        │
        ▼
 shader_compiler ──► shader_compilation / reflection / diagnostic
        │
        ▼
  .grshaderobj（工具私有、可缓存、不承诺跨版本兼容）
        │
        ▼
 shader_library_builder
        │
        ▼
  .grshlib（运行时发布单元）
        │
        ▼
 shader_library::create_shader(content_id)
```

## 非目标

- 不在 Renderer 或应用运行时引入源码编译、工具进程、文件系统或缓存策略。
- 不改变 `.grshlib` v1 布局、内容 ID、变体选择和运行时生命周期。
- 不把 DXC、Tint 或 SPIRV-Reflect 变成 Granit Core 的传递依赖。
- 不新增 Shader 语言、运行时后端、材质节点图、热更新调度或远程编译服务。
- 不把所有工具能力塞入一个持有全局状态的 Toolchain 对象。
- 不为尚未稳定的旧 ShaderTools API 保留转发入口或兼容别名。

## 已确认决策

### 共享类型只有一个来源

`include/granit/core/shader_types.h/.hpp` 保存 Renderer 与 ShaderTools 都需要的稳定 Shader 值类型：

- Shader stage；
- 源语言和代码格式；
- 后端目标位；
- `shader_content_id` 和 `shader_cache_key` 等领域别名。

`renderer/shader.h/.hpp` 和 ShaderTools 头文件引用这些类型，不再分别声明数值相同的枚举、宏和
摘要数组。类型头保持 C11 可独立包含，不依赖 Renderer 句柄或工具实现。

### 内容摘要实现属于 Core，领域名称保留语义

Shader、Texture、Environment、Material Archive 和 Pipeline Warmup 共用 SHA-256 实现。公共固定
长度值类型位于 `include/granit/core/content_id.h/.hpp`：`content_digest` 表示原始内容摘要，
`asset_content_id` 表示内容寻址的资产身份。Shader 与 Texture API 在此基础上保留
`shader_content_id`、`texture_content_id` 和 `shader_cache_key` 等领域名称，避免把载荷摘要、资产
身份和构建缓存身份混为同一概念。

私有算法实现位于 `src/core/sha256.*`，使用不带资源领域前缀的 `sha256_bytes()` 和
`sha256_bytes_with_zeroed_range()`。Shader 格式层只保留包含源码、入口、阶段、工具版本和编译选项
的缓存键生成规则。后端、资产格式和离线工具不得各自复制 SHA-256，也不得通过
`resource_content_id` 表示进程内 GPU Resource 身份。

### 一个 Compiler 接口覆盖所有源码语言

公开工具接口使用一个 Compiler 配置和一个编译描述：

```cpp
granit::shader_tools::compiler compiler;
compiler.initialize({.dxc_path = dxc, .tint_path = tint});

granit::shader_tools::compile_desc desc{
    .source_path = source,
    .source_language = granit::shader_source_language::hlsl,
    .stage = granit::shader_stage::fragment,
    .entry_point = "fragment_main",
    .defines = definitions,
    .target_backends = granit::shader_backend::all,
};

granit::shader_tools::compilation compilation;
const auto result = compiler.compile(desc, compilation);
```

Compiler 根据 `source_language` 选择内部前端：

- WGSL 使用 Tint 生成并验证 SPIR-V，同时保留规范化 WGSL；
- HLSL 使用 DXC 生成 SPIR-V，再由 Tint 生成 WGSL；

调用方不再选择 `compile_wgsl` 或 `compile_hlsl`。缺少所需工具配置时返回
`not_ready`，无效语言、阶段或目标位返回 `invalid_argument`。

### 编译结果、反射和序列化分开命名

`shader_tools::result` 改为移动独占的 `shader_tools::compilation`。它拥有一次编译的诊断、载荷和
反射生命周期，提供只读查询，不直接承担 Library 链接：

```cpp
compilation.info();
compilation.reflection();
compilation.spirv();
compilation.wgsl();
compilation.write_object(path);
```

反射值类型集中在 `shader_reflection.h/.hpp`。C++ 层使用强类型 stage、binding type、access 和
scalar type，不再把所有字段暴露为无含义的 `uint32_t`。独立 SPIR-V 检查返回同一套只读反射结构，
但不伪装成一次源码编译。

### Shader Object 是工具私有中间产物

`.grshader` 改名为 `.grshaderobj`，只用于编译缓存和 Library 链接。它可以包含阶段、入口、内容 ID、
缓存键、反射和各后端载荷摘要，但不安装到 Runtime component，也不承诺跨 Granit 版本兼容。

`write_object()` 负责从编译结果生成对象。已有 SPIR-V/WGSL 对的导入由独立 Object Builder 完成，
替代含义模糊的 `result_write_asset()` 和 CLI `pack` 内部实现。两条路径最终调用同一个对象编码器。

### Library Builder 独立于编译器

`shader_library_builder` 只接收已经验证的 `.grshaderobj` 或等价内存视图，并负责：

- 内容 ID 冲突检查；
- 后端载荷裁剪；
- 按摘要去重；
- 确定性排序与编码；
- 写入和检查 `.grshlib`。

Builder 不启动编译工具，也不决定源码语言。这样可以在一次构建中并行编译多个 Shader，再进行单次
Library 链接。

### CLI 和 CMake 只做适配

`granit_shader_tool` 保持一个可执行文件，但源码按命令拆分。`main.cpp` 只选择命令、解析公共选项并
映射退出码；编译、对象编码、缓存校验和 Library 链接必须调用 ShaderTools SDK。

CMake 模块只描述构建图、目标后端、输入输出和依赖，不解析二进制格式，也不复制编译器的缓存键
算法。相同源码、配置、工具身份和目标必须由 CLI、SDK 与 CMake 路径得到同一对象和 Library。

## 目标接口组织

公开头文件保持一个聚合入口，同时允许使用者只包含所需功能：

```text
include/granit/core/
  content_id.h
  content_id.hpp
  shader_types.h
  shader_types.hpp

include/granit/tools/
  shader_compiler.h
  shader_compiler.hpp
  shader_reflection.h
  shader_reflection.hpp
  shader_library_builder.h
  shader_library_builder.hpp
  shader_tools.h                 # C 聚合入口
  shader_tools.hpp               # C++ 聚合入口
```

安装目标继续只有 `granit::shader_tools`。拆分头文件不增加下游需要管理的链接目标，也不让工具依赖
传播到 `granit::granit`。

内部源码按职责组织：

```text
src/core/
  sha256.*

src/shader_format/
  shader_cache_key.*
  shader_object.*
  shader_library.*

tools/shader_compiler/
  compiler.*
  compilation.*
  reflection.*
  cache.*
  frontends/wgsl.*
  frontends/hlsl.*

tools/shader_library/
  builder.*
  storage.*

tools/shader_cli/
  main.cpp
  arguments.*
  commands/compile.cpp
  commands/inspect.cpp
  commands/object.cpp
  commands/library.cpp
  commands/targets.cpp
```

公共 C ABI 仍使用 `granit_shader_tools_` 前缀、整数句柄、版本化描述和结果码。C++20 包装只提供
强类型、移动语义和 RAII，不建立另一套编译状态。

## 实施顺序

1. **S-37K1 共享类型收敛（已完成）**：增加 Core Shader 类型头；迁移 Renderer、ShaderTools、
   格式编码器和测试；删除重复宏、枚举和固定长度数组别名。
2. **S-37K2 统一 Compiler API（已完成）**：引入 Compiler 配置、统一编译描述和单一 `compile()`；
   两种语言共享句柄生命周期、参数校验和结果创建，前端差异留在工具内部；同步 C11 与 C++20 API。
3. **S-37K2A 通用内容摘要收敛（已完成）**：增加 Core 内容摘要类型和私有 SHA-256 实现；迁移
   Shader、Texture、Environment、Material Archive、Pipeline Warmup 与离线工具；删除重复算法和
   领域错误的 `shader_bytes_sha256`，同时保留内容 ID、载荷摘要与缓存键的语义名称。
4. **S-37K3 Compilation 与 Reflection（已完成）**：将模糊的 Result 句柄拆成编译结果和反射视图；统一结构化
   Binding、接口变量、Workgroup 和 Override 查询；删除旧结果查询入口。
5. **S-37K4 Shader Object 边界（已完成）**：引入 `.grshaderobj`、Object Builder 和对象检查；
   迁移缓存、测试 Fixture 与生成规则；删除 `.grshader` 名称和旧
   `result_write_asset`/`restore_asset_cache` 组织。
6. **S-37K5 Library Builder 与 CLI 拆分（已完成）**：把 Library 编码、文件存储和命令实现移出
   `shader_tool_main.cpp`；CLI 成为薄适配层，CMake 只调用稳定命令接口。
7. **S-37K6 资产、安装与发布验收（本地完成）**：重建内建及公共 Shader Library，删除仓库中的旧中间快照，
   更新 Reference、Guide、迁移说明、安装清单和可复现 Toolchain 包。

每个阶段形成一个可独立评审的本地提交；七个阶段继续位于当前特性分支，不拆分 Pull Request。

## 测试与验收

- C11 与 C++20 公共头可分别独立包含，所有公开结构继续满足 ABI 布局检查。
- WGSL 和 HLSL 通过同一个 Compiler API 生成相同语义的 SPIR-V、WGSL 与反射结果。
- 缺少工具、无效源码、错误入口、阶段不匹配、无效 Define 和不支持目标均返回确定结果及诊断。
- Compiler 实例支持并行独立编译；Compilation 的字符串和载荷视图在对象销毁前稳定有效。
- `.grshaderobj` 覆盖截断、越界、摘要不匹配、未知版本、载荷缺失与冲突输入。
- 相同输入、工具身份、配置和目标逐字节生成相同 `.grshaderobj` 与 `.grshlib`。
- CLI、SDK 直接调用和 CMake 生成路径共享同一实现，并通过缓存命中与失效回归。
- 安装结果不包含 `.grshaderobj`、DXC、Tint 或工具私有头；Runtime 只安装 `.grshlib`。
- Windows 共享/静态、Linux GCC/Clang、Emscripten、浏览器 WebGPU、构建树和安装 Consumer 通过。
- `shader_cli/main.cpp` 只保留入口分派；单个命令实现和公共头不再承担多个领域职责。

## 风险与未决问题

- 共享类型迁移会同时影响 Core 与可选 ShaderTools ABI，必须一次更新头文件测试、导出检查和安装
  Consumer，不能在两个 component 中暂时保留重复定义。
- `compilation` 是否长期把载荷保存在内存中，应在 K2 用真实 PBR 批量构建测量；首版优先选择清晰
  所有权，不提前引入流式写入。
- `.grshaderobj` 不承诺跨版本兼容，但仍需版本、长度和摘要校验，避免缓存损坏变成未定义行为。
- 工具链包中的可执行文件、版本清单和许可证布局保持稳定；内部源码拆分不能改变下载和校验契约。
- 如果 C API 的 Builder 句柄不能带来真实编辑器或构建系统使用场景，可只公开一次性函数；C++ 层
  不应为了形式对称制造无必要的长期状态对象。
