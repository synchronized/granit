<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 离线工具

使用 `GRANIT_BUILD_TOOLS=ON` 构建可选离线工具：

```powershell
cmake --preset windows-clang-debug -DGRANIT_BUILD_TOOLS=ON
cmake --build --preset windows-clang-debug --target granit_shader_tool
```

`granit_shader_tool` 提供以下入口：

```powershell
granit_shader_tool inspect shader.spv
granit_shader_tool verify shader.spv
granit_shader_tool compile --tint path/to/tint --input shader.wgsl `
  --entry fragment_main --stage fragment --output shader.spv
```

`inspect` 按稳定顺序输出入口和资源绑定元数据；`verify` 执行低成本 SPIR-V 结构与反射检查；
`compile` 直接启动锁定版本的 Tint，捕获原始诊断并复核输出入口和阶段。完整 SPIR-V 合法性仍由
Tint 的 `--validate` 和可选 `spirv-val` 负责。工具不进入 Granit 核心动态库及安装导出。

`granit_material_tool inspect <package.grmat> --json` 验证最终二进制材质包并把稳定诊断 JSON 输出
到标准输出。使用 `--output <path>` 可以写入文件；Renderer 不读取该 JSON。

测试需要可执行的 `dxc`；若存在 `VULKAN_SDK`，CMake 会优先在其 `Bin` 目录查找 `dxc` 和
`spirv-val`。固定测试流程为 HLSL 经 DXC 编译到 Vulkan 1.3 SPIR-V、可选 `spirv-val` 校验，再由
SPIRV-Reflect 检查材质 Group 1 布局。
