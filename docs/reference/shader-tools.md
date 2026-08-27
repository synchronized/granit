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
- 参数字符串均为 UTF-8 的“指针 + 长度”，只需在调用期间有效，无需以零结尾。
- 参数和输出结构必须初始化 `struct_size`。未来版本只在结构体尾部追加字段。
- 参数有效后，即使编译或检查失败也可能返回非零结果句柄。调用者应读取 `status` 和诊断，最后
  调用 `granit_shader_tools_result_destroy`；C++ 包装会自动销毁。
- 查询得到的字符串视图由 SDK 持有，在结果句柄销毁前有效，调用者不得释放或修改。不得让查询
  与同一结果的销毁并发执行。
- 所有函数捕获内部异常，不允许异常穿过 C ABI。无效参数、无效句柄、内存不足和工具失败均以
  `granit_result` 返回。

首版结果提供入口点、Shader 阶段、标准输出和诊断文本。结构化绑定、资源类型及源位置诊断属于
S-10C3 反射 Schema 的后续范围。
