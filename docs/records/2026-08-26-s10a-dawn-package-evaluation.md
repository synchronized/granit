<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10A Dawn 发布包评估

## 结论

Dawn `v20260720.160313` 的 API 头与 Emdawnwebgpu 可以作为匹配原型基线，但官方 Windows Release
包不适合作为 Granit 的通用桌面运行时依赖。该包只提供静态库，并且对构建工具链版本敏感。
Granit 需要基于锁定修订自行构建 monolithic shared library，再验证动态加载。

## 验证输入

- Dawn 发布：`v20260720.160313`
- Dawn 修订：`0bc38adde72b79013536f8ce354b639ae19ae195`
- 发布标注的 Emscripten：emsdk `5.0.6`
- Windows Release SHA-256：
  `7af79f8525b15802d1438c6d2cd648cbea771c2cae56b43cae07870dd0f30130`
- Headers SHA-256：
  `b0b145903ab2df9e51e4cc074e155ac01342255c0ea101817b4e3a1d1fcde5f2`

两个下载文件的本地 SHA-256 均与发布元数据一致。

## 验证结果

- 包含 `dawn/webgpu.h`、`webgpu/webgpu.h` 和 CMake package 配置。
- Windows 包将 `dawn::webgpu_dawn` 声明为静态导入目标，只包含 `webgpu_dawn.lib`，没有 DLL。
- Clang GNU 风格前端因包配置传入 MSVC 专用 `-ignore:4221` 而无法链接。
- MSVC 19.41 可以编译 smoke test，但静态库引用了当前运行库未提供的 `__std_*` 与条件变量符号，
  表明预编译包与本机 MSVC/STL 工具集不匹配。
- 失败发生在链接阶段，尚未完成 Instance、Adapter 与 Device 运行验证。

## 后续处理

1. 在锁定的构建镜像中从同一 Dawn 修订构建 monolithic shared library。
2. 同时产出匹配的 headers、运行库、符号清单、许可证和 SHA-256 清单。
3. 用 MSVC 与 Clang 消费者只加载 C ABI DLL，不直接链接 Dawn 静态 C++ 实现。
4. 完成缺失库、缺失符号、Instance/Adapter/Device 和离屏回读测试后再接受 ADR-004。

仓库已增加 `Dawn Dependency Packages` 工作流。2026-08-26 的首次完整验证在 Windows 与 Linux
均成功：工作流从锁定修订构建 monolithic shared library，检查 `wgpuCreateInstance` 导出，并生成
带 SHA-256 的版本化压缩包。普通 push 不触发该重型构建；工作流文件变化时由 Pull Request 验证，
也可以手动触发。
