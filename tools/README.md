<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 离线工具

使用 `GRANIT_BUILD_TOOLS=ON` 构建可选离线工具：

```powershell
cmake --preset windows-clang-debug -DGRANIT_BUILD_TOOLS=ON `
  -DGRANIT_SHADER_TOOLCHAIN_ROOT=path/to/shader-toolchain
cmake --build --preset windows-clang-debug --target granit_asset_tool
```

统一工具链根目录的 `bin` 应包含锁定版本的 `dxc` 和 `tint`。这些程序只用于离线 Shader 资产
构建，不是应用运行时、Material、Texture 或 Environment 命令的依赖。

默认 `GRANIT_SHADER_TOOLCHAIN_POLICY=compatible`：版本不同会警告，但通过真实能力探测后仍可用。
官方可复现构建使用 `locked`，新版本试验可临时使用 `unchecked`。完整约束见
[AssetTools SDK](../docs/reference/asset-tools.md)。

`granit_asset_tool` 当前提供 Shader、Material、Texture 与 Environment 领域入口：

```powershell
granit_asset_tool shader inspect shader.spv
granit_asset_tool shader inspect --json shader.spv
granit_asset_tool shader verify shader.spv
granit_asset_tool shader targets
granit_asset_tool shader capabilities --target vulkan-portable
granit_asset_tool shader capabilities --target webgpu-portable
granit_asset_tool shader compile --toolchain path/to/granit-shader-toolchain `
  --input shader.hlsl --entry fragment_main --stage fragment `
  --define GRANIT_PBR_TEXTURE_MASK=31 --define GRANIT_PBR_LIGHTS=1 `
  --spirv-output shader.spv --wgsl-output shader.wgsl
granit_asset_tool material build material.grmat.json --output material.grmat `
  --shader-index library.grshidx.json
granit_asset_tool material inspect material.grmat --json
granit_asset_tool texture build --output texture.grtex --payload-output texture.bin `
  --dimension 2d --width 1024 --height 1024 --depth 1 --layers 1 --mips 11 `
  --variant rgba8-srgb=texture.rgba8.bin --variant bc7-rgba-srgb=texture.bc7.bin
granit_asset_tool texture inspect texture.grtex --json
granit_asset_tool environment build --irradiance irradiance.rgba16f.bin `
  --irradiance-resolution 32 --prefiltered 128=prefiltered-128.rgba16f.bin `
  --prefiltered 64=prefiltered-64.rgba16f.bin --brdf-lut brdf.rgba16f.bin `
  --brdf-width 256 --brdf-height 256 --output environment.grenv
granit_asset_tool environment inspect environment.grenv --json
```

`inspect` 按稳定顺序输出入口和资源绑定元数据；`inspect --json` 额外输出描述符、阶段接口、
Compute Workgroup 和 Override 常量的结构化调试视图；`verify` 执行低成本 SPIR-V 结构与反射检查；
完整 SPIR-V 合法性由 DXC、Tint 的 `--validate` 和可选 `spirv-val` 共同负责。工具不进入 Granit
核心动态库及安装导出。
所有调用都必须使用显式子命令；早期原型的单参数入口不再保留。
`targets` 列出工具内置的目标契约，`capabilities` 查询目标档位允许的可选特性。结果描述发布目标，
不读取构建机 GPU；当前两个 portable 目标都只包含基线能力，因此可选特性为 `none`。

`compile` 从统一 Toolchain 根目录调用 DXC 与 Tint，同时生成 Vulkan 1.3 SPIR-V 和 WebGPU
portable WGSL。
`--define NAME=VALUE` 可以重复；工具按名称排序后传给 DXC。
重复名称、非法标识符和空值会在启动编译器前失败。
Shader Object 写入与缓存只由 `build-library` 在内部管理。

`granit_asset_tool material inspect <package.grmat> --json` 验证最终二进制材质包并把稳定诊断 JSON 输出
到标准输出。使用 `--output <path>` 可以写入文件；Renderer 不读取该 JSON。

Texture `build` 接收一个或多个已经编码为目标 GPU 格式的完整紧密 mip/层负载，按 `--variant`
顺序生成 Manifest 和合并负载。它不解码图片或执行 BC、ETC2、ASTC 压缩。`inspect` 只检查
Manifest 并输出调试 JSON。

Environment `build` 接收预处理好的紧密 RGBA16F Cube 面、Prefiltered mip 链和 BRDF LUT，
生成带摘要的确定性 GRENV v3。它不解析 KTX2/PNG 或执行 HDR 卷积；这些步骤属于上游资产管线。

HLSL 双后端测试需要符合锁定契约的 DXC 与 `tint`；若存在 `VULKAN_SDK`，CMake 也会从其
`Bin` 目录查找 `dxc` 和 `spirv-val`。固定测试流程会生成 Vulkan 1.3 SPIR-V，
可选执行 `spirv-val`，并经 Tint 产生 WebGPU WGSL，最后检查两条路径的反射契约一致。
