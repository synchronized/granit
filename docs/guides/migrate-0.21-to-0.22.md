<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.21 迁移到 0.22

0.22.0 将 Shader、Material、Texture 和 Environment 的离线构建能力统一到可安装的 AssetTools
component 与 `granit_asset_tool`。运行时资产格式保持独立；项目仍处于 0.x，Consumer 应重新编译，
并把 CMake 请求版本更新为 0.22。

## 改用 AssetTools component

原 `ShaderTools` component、`granit::shader_tools` 目标和 `granit_shader_tool` 已删除。编辑器和资产
构建程序改为：

```cmake
find_package(granit 0.22 CONFIG REQUIRED COMPONENTS AssetTools)
target_link_libraries(asset_builder PRIVATE granit::asset_tools)
```

C API 使用 `<granit/tools/asset_tools.h>` 或对应领域头；C++ 使用 `asset_tools.hpp`。Shader 符号前缀
改为 `granit_asset_tools_shader_*`，C++ 命名空间改为 `granit::asset_tools::shader`。命令行统一为
`granit_asset_tool shader|material|texture|environment ...`。

## 使用统一 Shader Toolchain 根目录

Compiler 和 Library Builder 不再分别接收 DXC/Tint 路径，只接收包含 `bin/dxc` 与 `bin/tint` 的
Toolchain 根目录。CLI 使用 `--toolchain <root>`。CMake 配置可选择：

```sh
cmake -S . -B build \
  -DGRANIT_SHADER_TOOLCHAIN_MODE=system \
  -DGRANIT_SHADER_TOOLCHAIN_ROOT=<可选根目录>
```

`system` 默认不联网；`off` 禁用 HLSL 构建；`auto` 在本机工具不完整时下载锁定包；`download`
固定使用锁定缓存。安装 Consumer 可包含 `${granit_SHADER_TOOLCHAIN_MODULE}` 并调用
`granit_find_shader_toolchain()`。

## 迁移 Material、Texture 和 Environment 构建

- `.grmat` 源 JSON 与 Shader Library 索引交给 `granit_asset_tools_material_build`；旧的独立
  `granit_material_tool` 已删除。
- Texture Manifest 与合并负载交给 `granit_asset_tools_texture_build`。Core 中的
  `granit_texture_asset_encode` 和 C++ `encode_texture_asset` 已删除；运行时检查、变体选择和上传不变。
- GRENV v3 交给 `granit_asset_tools_environment_build`。Builder 接收预处理好的紧密 RGBA16F
  Irradiance Cube、Prefiltered mip 链和 BRDF LUT；旧 Model Viewer 私有环境工具已删除。

图片解码、GPU 纹理压缩和 HDR IBL 卷积仍由上游资产管线负责。Material、Texture 和 Environment
命令不需要 DXC、Tint，也不会触发 Shader Toolchain 下载。

## 更新输出与生命周期处理

各 Builder 返回所属领域的移动独占结果句柄。成功时从句柄读取最终资产和调试 JSON；语义错误时
可读取诊断。所有视图只在结果句柄销毁前有效。CLI 只在构建成功后原子替换输出，调用方不应解析
人类可读的标准错误文本作为机器协议。
