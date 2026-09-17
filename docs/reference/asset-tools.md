<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# AssetTools SDK

AssetTools 是供编辑器、资产构建器和命令行工具直接链接的可选组件。Shader 领域通过 Compiler
调用锁定版本的 DXC 与 Tint，将 HLSL 编译为离线 Shader 产物，并检查 SPIR-V 的入口点、阶段和
反射。Material 领域从源 JSON 与 Shader 逻辑索引构建和检查 `.grmat`。Texture 领域从已经编码的
GPU 格式变体生成 Manifest 与合并负载。Environment 领域从预处理 RGBA16F 像素构建和检查
`.grenv`。AssetTools 不进入核心渲染库的传递依赖。

## 构建与链接

配置时启用 `GRANIT_BUILD_ASSET_TOOLS=ON`，安装后通过独立组件链接：

```cmake
find_package(granit CONFIG REQUIRED COMPONENTS AssetTools)
target_link_libraries(editor PRIVATE granit::asset_tools)
```

`GRANIT_BUILD_TOOLS=ON` 也会构建该 SDK，因为 `granit_asset_tool` 是它的命令行薄适配层。Shader
Compiler 只接收工具链根目录，按固定的 `bin/dxc` 与 `bin/tint` 布局解析工具，不会形成公共链接
依赖。

HLSL portable 路径需要资产构建机安装 DXC 与 Tint，但应用运行时和 Granit 核心 SDK 均不需要
它们。标准 SDK、CLI 和 CMake 资产入口统一使用 `GRANIT_SHADER_TOOLCHAIN_ROOT`，其 `bin` 目录
必须包含两个工具。配置模块仍可探测单独安装的 DXC 与 Tint，以报告本机能力；没有统一根目录时
不会启动跨后端 HLSL 资产构建。配置阶段会检查锁定版本及 Tint 所需转换能力，不符合契约的工具
不会用于端到端测试。

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

`GRANIT_SHADER_TOOLCHAIN_MODE` 控制工具链来源：

- `off` 禁用 HLSL 构建且不查找工具、不访问网络；Material、Texture 和 Environment 仍可使用；
- `system` 只搜索显式 `GRANIT_SHADER_TOOLCHAIN_ROOT`、Vulkan SDK 和 `PATH`；
- `auto` 是默认值，先执行 `system` 搜索，缺少完整 DXC/Tint 时下载锁定发布包；
- `download` 忽略偶然发现的系统工具并使用锁定缓存包。显式 Root 在所有模式中优先。

下载缓存由 `GRANIT_SHADER_TOOLCHAIN_CACHE_DIR` 指定，默认位于构建树。下载过程使用进程锁、临时
归档、发布 SHA-256、包内逐文件清单校验和原子目录替换。安装 AssetTools 后可通过
`granit_SHADER_TOOLCHAIN_MODULE` 引入同一 `GranitShaderToolchain.cmake` 模块。

Library Builder 自动将 DXC 与 Tint 二进制的 SHA-256 身份纳入缓存键，因此路径相同但二进制升级
后不会复用旧产物。工具身份属于缓存实现，不单独暴露公共查询接口。

## 接口与生命周期

Compiler、Compilation、Reflection 及 Material、Texture、Environment 构建结果均使用独立的 64 位
不透明句柄。零值无效；把一种句柄传给另一种结果 API、重复销毁或使用已销毁句柄，会返回
`GRANIT_ERROR_INVALID_HANDLE`。句柄编码包含内部类型、槽位和 generation，数值不可作为资产内容
ID，也不可保存到文件或跨进程使用。不同结果的查询仍可并行；销毁同一句柄前，调用者须完成使用
该句柄的查询。

- C11 的编译、反射和 Library Builder 入口分别位于对应的
  `<granit/asset_tools/shader_*.h>`；`.hpp` 提供 C++20 包装。`asset_tools.h/.hpp` 是 AssetTools 的聚合
  入口，后续资产领域继续使用各自独立头文件。
- `granit_asset_tools_shader_compiler_create` 创建可复用 Compiler，配置只包含 Toolchain 根目录；
  `granit_asset_tools_shader_compiler_compile` 固定接收 HLSL。C++ 包装对应移动独占的 `compiler` 和
  `compile_desc`，编译描述不再携带源码语言。
- Compiler 通过 DXC 生成 Vulkan SPIR-V，再由 Tint 生成 portable WGSL。编译描述始终要求
  SPIR-V 与 WGSL 输出路径；`target_backends` 声明后续产物面向的非零后端集合。
- Compiler 在创建时复制根目录并验证固定工具布局，可供多个线程并发编译；销毁必须等待使用该
  句柄的调用结束。缺少 DXC 或 Tint 时创建返回 `not_ready`，空根目录、未知阶段、目标位或无效
  路径返回 `invalid_argument`。`granit_asset_tools_shader_inspect_spirv` 独立检查已有 SPIR-V，
  并直接返回 Reflection，不创建 Compilation。
- `granit_asset_tools_shader_compilation` 持有一次源码编译的状态、诊断和 SPIR-V/WGSL 载荷；
  `granit_asset_tools_shader_compilation_get_reflection` 返回独立拥有的 Reflection，销毁 Compilation 后仍可
  查询。两种句柄由不同句柄表校验，不能混用。
- Library Builder 在启动编译器前校验输入、编译上下文和 Object 摘要；命中时从私有 sidecar
  恢复所需产物，未命中时编译并原子更新缓存。Object 写入和缓存恢复不属于公共 API。
- 当前 sidecar 分别代表 WebGPU portable WGSL 和 Vulkan portable SPIR-V。资产按后端打包裁剪和
  多能力档位选择属于 [S-20](../plans/S-20-shader-asset-variants.md)。
- `.grshlib` 是 Core 的公共运行时资产；`.grshaderobj` 的链接、裁剪、去重和原子写入只由
  Library Builder 在工具内部执行。
- `granit_asset_tools_shader_build_library_from_manifest` 是 HLSL-first 的高层构建入口。它读取
  `.grshlib.json`，按逻辑名称和变体编译 HLSL，管理私有 `.grshaderobj` 缓存，并一次写出
  `.grshlib` 与 `.grshidx.json`。C++ 包装使用 `source_library_desc`；CLI 对应 `build-library`。
  `cache_hit` 只在全部 Object、Library 和索引均未变化时为真。
- CMake 的 `granit_add_hlsl_shader_library` 接收 `MANIFEST`、`SOURCES`、`OUTPUT`、`INDEX` 和
  `CACHE_DIR`。`SOURCES` 只声明构建依赖；清单解析、变体规范化、缓存身份和资产编码仍由
  AssetTools 处理。可选的 `REFERENCE` 与 `INDEX_REFERENCE` 用于逐字节校验发布快照。
- CLI 的 `index-ids` 从 `.grshidx.json` 按逻辑名称生成 C++ 内容 ID 常量，供内嵌资产代码使用；
  它不读取或暴露私有 `.grshaderobj` 缓存路径。
- HLSL portable 路径让 DXC 直接生成最终 Vulkan 1.3 SPIR-V；另行生成临时 Vulkan 1.1 /
  SPIR-V 1.3 中间文件供锁定 Tint 的 SPIR-V Reader 转换 WGSL，并要求两份 SPIR-V 的反射契约
  一致。临时文件不会进入资产。DXC 或 Tint 拒绝源代码及其能力时，调用返回
  `initialization_failed`、保留工具诊断并删除不完整产物，不会降低 Vulkan sidecar 的目标版本，
  也不会静默降级为仅 Vulkan 资产。
- `granit_asset_tools_shader_compile_desc.defines` 接收显式长度的名称和值。名称必须是合法标识符，
  值不能为空，同名定义会被拒绝；SDK 按名称排序后传给 DXC。CLI 对应参数为可重复的
  `--define NAME=VALUE`。排序后的完整定义集合属于编译上下文并进入缓存键。
- 命令行 `compile` 用于单次 HLSL 编译和诊断，只输出 SPIR-V 与 WGSL。正式资产通过
  `build-library` 构建，由 Builder 管理工具身份、Object 缓存和最终 Library。
- HLSL-first Library Builder 的私有 Object 缓存键基于源码内容、入口点、阶段、工具修订号、目标、
  选项和必需特性。
- `granit_asset_tools_shader_get_target_capabilities` 查询工具内置目标档位的静态契约，不读取构建机 GPU。
  CLI 的 `targets` 列出目标，`capabilities --target <name>` 查询对应能力。当前提供
  `vulkan-portable` 和 `webgpu-portable`，二者均不声明额外可选特性。
- Library Builder 将目标后端和必需特性纳入缓存键与变体记录。
- `granit_asset_tools_shader_index_find_content_id` 从内存中的 `.grshidx.json` 查询逻辑 Shader 名称，
  供 CLI 和上游资产管线生成稳定内容 ID 引用，无需访问 SDK 私有 JSON 类型。
- `granit_asset_tools_shader_reflection_get_binding_count` 和
  `granit_asset_tools_shader_reflection_get_binding` 按
  Group、Binding 数字顺序返回结构化绑定。记录包含资源类型、访问模式、数组数量和 Buffer
  最小绑定尺寸。
- Vertex 输入和 Fragment 输出按 Location、Component 排序，记录标量类型、位宽及向量宽度；
  Compute 入口点可查询固定 Workgroup 的 X/Y/Z 大小。内建接口变量不会进入用户接口列表。
- Override／Specialization Constant 按常量 ID 排序，记录名称、标量类型、位宽和默认值原始位模式；
  `default_value_size` 指明原始值占用的有效字节数。
- `granit_asset_tools_shader_reflection_get_json` 返回与结构化查询字段一致、稳定排序的 UTF-8 JSON；
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
  调用 `granit_asset_tools_shader_compilation_destroy` 或
  `granit_asset_tools_shader_reflection_destroy`；C++ 包装
  会自动销毁。
- 查询得到的字符串和载荷视图由 SDK 持有，在所属句柄销毁前有效，调用者不得释放或修改。不得让
  查询与同一句柄的销毁并发执行。
- 所有函数捕获内部异常，不允许异常穿过 C ABI。无效参数、无效句柄、内存不足和工具失败均以
  `granit_result` 返回。

## Material Builder

- C11 入口位于 `<granit/asset_tools/material_builder.h>`，C++20 包装位于对应 `.hpp`，命名空间为
  `granit::asset_tools::material`。
- `granit_asset_tools_material_build` 接收 Material 源 JSON 和一个或多个内存中的
  `.grshidx.json`，在 SDK 内将逻辑 Shader 名称解析为内容 ID，并生成确定性的 `.grmat` 与稳定
  调试 JSON。最终资产不保存索引路径。
- `granit_asset_tools_material_inspect` 从内存检查已有 `.grmat`，使用与 Runtime 相同的格式实现。
  build 和 inspect 均返回移动独占的结果句柄；Archive、调试 JSON 和诊断视图在句柄销毁前有效。
- 描述结构无效时不创建结果；输入内容或资产语义无效时返回 `invalid_argument`，并尽量返回包含
  诊断的结果句柄。CLI 把诊断写入标准错误，并只在构建成功后原子替换输出文件。
- CLI 使用 `granit_asset_tool material build` 和 `granit_asset_tool material inspect`。Material
  领域不查找、加载或下载 Shader Toolchain。

## Texture Builder

- C11 入口位于 `<granit/asset_tools/texture_builder.h>`，C++20 包装位于对应 `.hpp`，命名空间为
  `granit::asset_tools::texture`。
- Builder 接收逻辑尺寸、按偏好排序的格式变体和每个变体的已编码负载。调用方可以提供显式
  子资源布局；同时省略子资源指针和数量时，Builder 根据尺寸、层数和 mip 数生成紧密布局。它按
  变体顺序拼接负载，计算各负载 SHA-256，并根据尺寸、格式、用途、布局和摘要生成内容 ID；调用方
  不再填写负载偏移、摘要或内容 ID。
- 内容 ID 使用固定的 `granit.texture.asset.v1` 域和小端规范化字段计算。相同输入得到相同
  Manifest、合并负载与内容 ID；改变变体顺序、格式、用途、布局或负载都会改变身份。
- `granit_asset_tools_texture_inspect` 检查 Manifest 并提供稳定调试 JSON。Builder 和 Inspector
  都不创建 GPU 对象，也不依赖 Renderer 状态或 Shader Toolchain。
- CLI 的 `texture build` 接受 `--variant <format=payload>`，当前要求每个文件按 layer、mip 顺序
  紧密保存完整链；`texture inspect` 输出 JSON。图片解码和 GPU 格式压缩仍由上游资产管线负责。

## Environment Builder

- C11 入口位于 `<granit/asset_tools/environment_builder.h>`，C++20 包装位于对应 `.hpp`，命名空间为
  `granit::asset_tools::environment`。
- Builder 接收紧密排列的 RGBA16F Irradiance Cube、完整 Prefiltered Cube mip 链、BRDF LUT
  以及推荐环境强度和曝光值，生成包含 SHA-256 负载摘要的确定性 GRENV v3 包。
- 六个 Cube 面在每个输入中连续排列；Prefiltered mip 必须从给定的二次幂分辨率逐级缩小，且
  每个 mip 的像素字节数必须与分辨率严格一致。Builder 不执行 HDR 卷积、图片解码或格式转换。
- `granit_asset_tools_environment_inspect` 使用与 Runtime 相同的私有格式实现检查版本、布局、参数
  和摘要，并输出稳定调试 JSON。Builder 与 Inspector 均不创建 GPU 对象或依赖 Shader Toolchain。
- CLI 使用 `granit_asset_tool environment build` 和 `environment inspect`。旧的 Model Viewer 私有
  打包工具已删除，输入容器解析与 RGBA16F 预处理由上游资产管线负责。

命令行可使用 `granit_asset_tool shader inspect --json shader.spv` 输出稳定排序的 JSON 调试视图。普通
`inspect` 的 CSV 文本保持兼容，但程序不应解析该文本，应使用结构化 SDK 查询或反射 JSON 视图。

Compilation 提供编译状态、入口点、Shader 阶段、标准输出、诊断以及编译载荷；Reflection 提供
描述符绑定、Vertex 输入、Fragment 输出、Compute Workgroup 和 Override 常量。SDK 已提供 WGSL
预期 Binding 与 SPIR-V 的严格集合校验；
自动提取预期集合仍需接入锁定 Tint 的可验证机器输出，不使用 WGSL 源码正则解析。结构化源位置
诊断仍属于后续范围。
