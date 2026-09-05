<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 离线工具

使用 `GRANIT_BUILD_TOOLS=ON` 构建可选离线工具：

```powershell
cmake --preset windows-clang-debug -DGRANIT_BUILD_TOOLS=ON `
  -DGRANIT_SHADER_TOOLCHAIN_ROOT=path/to/shader-toolchain
cmake --build --preset windows-clang-debug --target granit_shader_tool
```

统一工具链根目录的 `bin` 应包含锁定版本的 `dxc` 和 `tint`。也可以分别传入
`GRANIT_DXC_EXECUTABLE` 与 `GRANIT_TINT_EXECUTABLE`；这些程序只用于离线资产构建，不是应用
运行时依赖。

`granit_shader_tool` 提供以下入口：

```powershell
granit_shader_tool inspect shader.spv
granit_shader_tool inspect --json shader.spv
granit_shader_tool verify shader.spv
granit_shader_tool targets
granit_shader_tool capabilities --target vulkan-portable
granit_shader_tool capabilities --target webgpu-portable
granit_shader_tool compile --tint path/to/tint --input shader.wgsl `
  --entry fragment_main --stage fragment --output shader.spv
granit_shader_tool compile --tint path/to/tint --input shader.wgsl `
  --entry fragment_main --stage fragment --output shader.spv `
  --asset shader.granit-shader --tint-revision dawn-v20260720.160313 `
  --asset-backend all
granit_shader_tool compile-hlsl --dxc path/to/dxc --tint path/to/tint `
  --input shader.hlsl --entry fragment_main --stage fragment `
  --spirv-output shader.spv --wgsl-output shader.wgsl `
  --asset shader.granit-shader --dxc-revision <revision> --tint-revision <revision>
```

`inspect` 按稳定顺序输出入口和资源绑定元数据；`inspect --json` 额外输出描述符、阶段接口、
Compute Workgroup 和 Override 常量的结构化调试视图；`verify` 执行低成本 SPIR-V 结构与反射检查；
`compile` 直接启动锁定版本的 Tint，捕获原始诊断并复核输出入口和阶段。完整 SPIR-V 合法性仍由
Tint 的 `--validate` 和可选 `spirv-val` 负责。指定 `--asset` 时还需要提供锁定的
`--tint-revision`。工具会将稳定反射清单写入指定路径，并生成同名 `.wgsl` 与 `.spv` sidecar；
目标环境默认记录为 `vulkan1.3`。工具不进入 Granit 核心动态库及安装导出。
`--asset-backend` 可取 `all`、`vulkan` 或 `webgpu`，默认 `all`；未选择的同名 sidecar 会被删除，
清单不会声明未随包交付的变体。
`--features` 可声明 `none`、`float16` 或 `subgroup`。当前 portable 目标只接受 `none`；请求其他
特性会在启动编译器前失败并指出缺失特性。
ShaderTools SDK 还可接收调用方从 WGSL 前端取得的预期 Group/Binding 集合，并与最终 SPIR-V
严格比较；当前 CLI 尚未自行提取该集合。
所有调用都必须使用显式子命令；早期原型的单参数入口不再保留。
`targets` 列出工具内置的目标契约，`capabilities` 查询目标档位允许的可选特性。结果描述发布目标，
不读取构建机 GPU；当前两个 portable 目标都只包含基线能力，因此可选特性为 `none`。

`compile-hlsl` 调用显式提供的 DXC 与 Tint，同时生成 Vulkan 1.3 SPIR-V 和 WebGPU portable WGSL。
使用 `--asset` 时必须记录两项工具版本；`--asset-backend` 可按发布目标裁剪 sidecar。全后端资产
缓存以原始 HLSL 和完整编译上下文为身份，命中时会在启动 DXC/Tint 前恢复两个产物；单后端裁剪
暂不执行编译前恢复，因为资产未保存另一后端的输出。

`granit_material_tool inspect <package.grmat> --json` 验证最终二进制材质包并把稳定诊断 JSON 输出
到标准输出。使用 `--output <path>` 可以写入文件；Renderer 不读取该 JSON。

HLSL 双后端测试需要符合锁定契约的 `dxc` 和 `tint`；若存在 `VULKAN_SDK`，CMake 也会从其
`Bin` 目录查找 `dxc` 和 `spirv-val`。固定测试流程为 HLSL 经 DXC 编译到 Vulkan 1.3 SPIR-V、
可选 `spirv-val` 校验，并经 Tint 产生 WebGPU WGSL，最后检查两条路径的反射契约一致。
