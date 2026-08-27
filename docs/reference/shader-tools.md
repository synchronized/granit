<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ShaderTools SDK

ShaderTools 是供编辑器、资产构建器和命令行工具直接链接的可选组件。它负责调用锁定版本的 Tint
将 WGSL 编译为 Vulkan SPIR-V，并检查 SPIR-V 的入口点、阶段和当前反射文本；它不进入核心渲染库
的传递依赖。

## 构建与链接

配置时启用 `GRANIT_BUILD_SHADER_TOOLS=ON`，安装后通过独立组件链接：

```cmake
find_package(granit 0.3 CONFIG REQUIRED COMPONENTS ShaderTools)
target_link_libraries(editor PRIVATE granit::shader_tools)
```

`GRANIT_BUILD_TOOLS=ON` 也会构建该 SDK，因为 `granit_shader_tool` 是它的命令行薄适配层。Tint
可执行文件仍是编译调用的显式输入，不会成为公共链接依赖。

## 接口与生命周期

- C11 入口位于 `<granit/tools/shader_tools.h>`；C++20 RAII 包装位于对应 `.hpp`。
- `granit_shader_tools_compile_wgsl` 编译 WGSL；`granit_shader_tools_inspect_spirv` 检查 SPIR-V。
- `granit_shader_tools_result_get_binding_count` 和 `granit_shader_tools_result_get_binding` 按
  Group、Binding 数字顺序返回结构化绑定。记录包含资源类型、访问模式、数组数量和 Buffer
  最小绑定尺寸。
- Vertex 输入和 Fragment 输出按 Location、Component 排序，记录标量类型、位宽及向量宽度；
  Compute 入口点可查询固定 Workgroup 的 X/Y/Z 大小。内建接口变量不会进入用户接口列表。
- 参数字符串均为 UTF-8 的“指针 + 长度”，只需在调用期间有效，无需以零结尾。
- 参数和输出结构必须初始化 `struct_size`。未来版本只在结构体尾部追加字段。
- 参数有效后，即使编译或检查失败也可能返回非零结果句柄。调用者应读取 `status` 和诊断，最后
  调用 `granit_shader_tools_result_destroy`；C++ 包装会自动销毁。
- 查询得到的字符串视图由 SDK 持有，在结果句柄销毁前有效，调用者不得释放或修改。不得让查询
  与同一结果的销毁并发执行。
- 所有函数捕获内部异常，不允许异常穿过 C ABI。无效参数、无效句柄、内存不足和工具失败均以
  `granit_result` 返回。

命令行可使用 `granit_shader_tool inspect --json shader.spv` 输出稳定排序的 JSON 调试视图。普通
`inspect` 的 CSV 文本保持兼容，但程序不应解析该文本，应使用结构化 SDK 接口。

当前结果提供入口点、Shader 阶段、描述符绑定、Vertex 输入、Fragment 输出、Compute Workgroup、
标准输出和诊断文本。Override 常量、WGSL/SPIR-V 集合一致性及结构化源位置诊断仍属于 S-10C3
的后续范围。
