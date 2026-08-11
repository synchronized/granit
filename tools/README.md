<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 离线工具

使用 `GRANIT_BUILD_TOOLS=ON` 构建可选离线工具：

```powershell
cmake --preset windows-clang-debug -DGRANIT_BUILD_TOOLS=ON
cmake --build --preset windows-clang-debug --target granit_shader_tool
```

`granit_shader_tool <shader.spv>` 读取 SPIR-V，并按稳定顺序输出入口和资源绑定元数据。当前工具是
H-02D 原型，不定义长期稳定的命令行或输出格式，也不进入 Granit 核心动态库及安装导出。

测试需要可执行的 `dxc`；若存在 `VULKAN_SDK`，CMake 会优先在其 `Bin` 目录查找 `dxc` 和
`spirv-val`。固定测试流程为 HLSL 经 DXC 编译到 Vulkan 1.3 SPIR-V、可选 `spirv-val` 校验，再由
SPIRV-Reflect 检查材质 Group 1 布局。
