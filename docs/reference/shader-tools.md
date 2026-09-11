<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ShaderTools SDK

ShaderTools 是供编辑器、资产构建器和命令行工具直接链接的可选组件。它通过统一 Compiler 调用
锁定版本的 DXC 与 Tint，将 WGSL 或 HLSL 编译为离线 Shader 产物，并检查
SPIR-V 的入口点、阶段和反射；它不进入核心渲染库的传递依赖。

## 构建与链接

配置时启用 `GRANIT_BUILD_SHADER_TOOLS=ON`，安装后通过独立组件链接：

```cmake
find_package(granit 0.3 CONFIG REQUIRED COMPONENTS ShaderTools)
target_link_libraries(editor PRIVATE granit::shader_tools)
```

`GRANIT_BUILD_TOOLS=ON` 也会构建该 SDK，因为 `granit_shader_tool` 是它的命令行薄适配层。编译器
可执行文件路径在创建 Compiler 时配置并复制到句柄中，不会成为公共链接依赖。

HLSL portable 路径需要资产构建机安装 DXC 与 Tint，但应用运行时和 Granit 核心 SDK 均不需要
它们。配置时优先设置 `GRANIT_SHADER_TOOLCHAIN_ROOT`，其 `bin` 目录应包含两个工具；也可分别
设置 `GRANIT_DXC_EXECUTABLE` 与 `GRANIT_TINT_EXECUTABLE`。未设置统一根目录时，DXC 还会从
`VULKAN_SDK` 和 PATH 查找，Tint 从 PATH 查找。配置阶段会检查锁定版本及 Tint 所需转换能力，不符合契约的工具不会用于
端到端测试。

当前 Windows 工具链契约锁定 DXC `1.8.0.4973`，Linux 工具链契约锁定 DXC 构建
`4973-8f559587`。两者均锁定 Dawn/Tint
`v20260720.160313`（修订
`0bc38adde72b79013536f8ce354b639ae19ae195`）。资产会记录真实工具身份，使缓存身份不依赖开发机
路径。工具链是离线依赖，不进入 `granit::granit` 的安装导出或传递依赖。

`GRANIT_SHADER_TOOLCHAIN_POLICY` 控制配置期接受规则：

- `compatible` 是默认值。锁定版本直接接受，其他可启动版本发出警告并继续执行真实编译能力探测；
  只有能力探测失败才禁用工具。
- `locked` 用于官方 CI 和发布构建。DXC 必须匹配锁定版本，Tint 还必须通过
  `GRANIT_TINT_REVISION` 提供匹配的源码修订，否则配置失败。
- `unchecked` 只用于适配新工具链，跳过版本和配置期编译能力约束；真实资产编译仍可能失败。

`granit_shader_tools_get_tool_identity` 返回指定工具二进制的 SHA-256。CLI 在未提供
`--dxc-revision` 或 `--tint-revision` 时自动使用该身份构造缓存键，因此路径
相同但二进制升级后不会复用旧产物。显式修订号仍适用于可复现构建，以及编译器暂时不可用但需要
从已有资产恢复 sidecar 的流程。

## 接口与生命周期

- C11 的编译和反射入口分别位于 `<granit/tools/shader_compiler.h>` 与
  `<granit/tools/shader_reflection.h>`；对应 `.hpp` 提供 C++20 RAII 包装。`shader_tools.h/.hpp`
  只作为聚合入口。
- `granit_shader_tools_compiler_create` 创建可复用 Compiler，配置包含 DXC 与 Tint 路径；
  `granit_shader_tools_compiler_compile` 通过 `source_language` 选择前端。C++ 包装对应移动独占的
  `compiler` 和统一 `compile_desc`。旧的语言专用描述与编译入口已删除。
- WGSL 通过 Tint 生成 portable SPIR-V；HLSL 通过 DXC 生成 SPIR-V，再由 Tint 生成 portable
  WGSL。统一编译描述始终要求 SPIR-V 输出路径，HLSL 还要求 WGSL
  输出路径。`target_backends` 声明后续产物面向的非零后端集合。
- Compiler 在创建时复制工具路径，可供多个线程并发编译；销毁必须等待使用该句柄的调用结束。
  缺少当前语言所需工具配置时，编译返回 `not_ready`，未知语言、阶段、目标位或无效路径返回
  `invalid_argument`。`granit_shader_tools_inspect_spirv` 独立检查已有 SPIR-V，并直接返回 Reflection，
  不创建 Compilation。
- `granit_shader_tools_compilation` 持有一次源码编译的状态、诊断和 SPIR-V/WGSL 载荷；
  `granit_shader_tools_compilation_get_reflection` 返回独立拥有的 Reflection，销毁 Compilation 后仍可
  查询。两种句柄由不同句柄表校验，不能混用。
- `granit_shader_tools_restore_asset_cache` 在启动 Tint 前校验输入、编译上下文和资产摘要；命中时
  从 sidecar 恢复所需产物；清单或任一 sidecar 不存在、损坏及缓存键变化均作为正常未命中。
- `granit_shader_tools_compilation_write_asset` 将稳定反射和载荷摘要写入 `.granit-shader` 清单，并将
  WGSL、SPIR-V 写入同名 `.wgsl`、`.spv` sidecar。只有三个文件均逐字节相同时才报告缓存命中。
- 当前 sidecar 分别代表 WebGPU portable WGSL 和 Vulkan portable SPIR-V。资产按后端打包裁剪和
  多能力档位选择属于 [S-20](../plans/S-20-shader-asset-variants.md)。
- 命令行 `library` 可将多个已验证 Shader Asset 确定性链接为 `.grshlib`，按 SHA-256 去重
  载荷，并以 `--target all|vulkan|webgpu` 生成全后端或裁剪结果。`.grshlib` 是 Core 的公共运行时
  资产；`.grshader` 与 sidecar 只保留在工具私有的中间产物或缓存中。
- HLSL portable 路径让 DXC 直接生成最终 Vulkan 1.3 SPIR-V；另行生成临时 Vulkan 1.1 /
  SPIR-V 1.3 中间文件供锁定 Tint 的 SPIR-V Reader 转换 WGSL，并要求两份 SPIR-V 的反射契约
  一致。临时文件不会进入资产。DXC 或 Tint 拒绝源代码及其能力时，调用返回
  `initialization_failed`、保留工具诊断并删除不完整产物，不会降低 Vulkan sidecar 的目标版本，
  也不会静默降级为仅 Vulkan 资产。
- `granit_shader_tools_compile_desc.defines` 仅用于 HLSL，接收显式长度的名称和值。名称必须是合法标识符，
  值不能为空，同名定义会被拒绝；SDK 按名称排序后传给 DXC。CLI 对应参数为可重复的
  `--define NAME=VALUE`。排序后的完整定义集合属于编译上下文并进入缓存键。
- 命令行 `compile-hlsl` 暴露相同路径，并可直接写入、裁剪 `.granit-shader` 资产。写资产时必须
  显式记录 DXC 与 Tint 修订号。全后端资产缓存命中时会在启动两个编译器前直接恢复 SPIR-V 和
  WGSL；单后端裁剪目前仍执行完整编译，避免声称恢复了未被资产保存的另一后端产物。
- 缓存键基于原始源码语言、原始源码内容、入口点、阶段、工具修订号、目标、选项和必需特性。
  因此相同文本分别作为 WGSL 或 HLSL 输入时不会错误共享缓存。
- `granit_shader_tools_asset_desc.backend_mask` 必须选择 Vulkan、WebGPU 或二者；写入时会删除同名
  的未选后端 sidecar，清单仅记录实际保留的变体。缓存描述的 `backend_mask` 表示期望的精确
  变体集合，清单集合不同也会正常未命中；两个字段均不能为零。
- `granit_shader_tools_get_target_capabilities` 查询工具内置目标档位的静态契约，不读取构建机 GPU。
  CLI 的 `targets` 列出目标，`capabilities --target <name>` 查询对应能力。当前提供
  `vulkan-portable` 和 `webgpu-portable`，二者均不声明额外可选特性。
- 资产和缓存描述的 `required_features` 会进入缓存键和变体记录。未知特性位返回
  `invalid_argument`；任一所选目标档位不支持必需特性时，写入返回 `unsupported` 且不修改资产。
- `granit_shader_tools_reflection_get_binding_count` 和 `granit_shader_tools_reflection_get_binding` 按
  Group、Binding 数字顺序返回结构化绑定。记录包含资源类型、访问模式、数组数量和 Buffer
  最小绑定尺寸。
- Vertex 输入和 Fragment 输出按 Location、Component 排序，记录标量类型、位宽及向量宽度；
  Compute 入口点可查询固定 Workgroup 的 X/Y/Z 大小。内建接口变量不会进入用户接口列表。
- Override／Specialization Constant 按常量 ID 排序，记录名称、标量类型、位宽和默认值原始位模式；
  `default_value_size` 指明原始值占用的有效字节数。
- `granit_shader_tools_reflection_get_json` 返回与结构化查询字段一致、稳定排序的 UTF-8 JSON；
  C++ 包装通过 `reflection::reflection_json()` 提供只读视图。Binding 类型、访问模式和标量类型在
  C++ 中使用强类型枚举。
  C++ 查询和构建函数使用 `granit::result` 返回操作状态，可通过 `ok()`、`failed()` 或显式布尔
  上下文判断；底层 C API 继续返回 `granit_result`。
- 参数字符串均为 UTF-8 的“指针 + 长度”，只需在调用期间有效，无需以零结尾。
- 参数和输出结构必须初始化 `struct_size`。未来版本只在结构体尾部追加字段。
- 统一编译描述或检查描述可设置 `validate_binding_set=1`，并传入从 WGSL 前端获得的
  `expected_bindings`。SDK 会按 Group/Binding 比较最终 SPIR-V；缺失、多余或重复记录都会失败，
  编译失败时删除输出文件。零值关闭该检查。
- 参数有效后，即使编译或检查失败也可能返回非零句柄。调用者应读取 `status` 和诊断，最后按类型
  调用 `granit_shader_tools_compilation_destroy` 或 `granit_shader_tools_reflection_destroy`；C++ 包装
  会自动销毁。
- 查询得到的字符串和载荷视图由 SDK 持有，在所属句柄销毁前有效，调用者不得释放或修改。不得让
  查询与同一句柄的销毁并发执行。
- 所有函数捕获内部异常，不允许异常穿过 C ABI。无效参数、无效句柄、内存不足和工具失败均以
  `granit_result` 返回。

命令行可使用 `granit_shader_tool inspect --json shader.spv` 输出稳定排序的 JSON 调试视图。普通
`inspect` 的 CSV 文本保持兼容，但程序不应解析该文本，应使用结构化 SDK 查询或反射 JSON 视图。

Compilation 提供编译状态、入口点、Shader 阶段、标准输出、诊断以及编译载荷；Reflection 提供
描述符绑定、Vertex 输入、Fragment 输出、Compute Workgroup 和 Override 常量。SDK 已提供 WGSL
预期 Binding 与 SPIR-V 的严格集合校验；
自动提取预期集合仍需接入锁定 Tint 的可验证机器输出，不使用 WGSL 源码正则解析。结构化源位置
诊断仍属于后续范围。
