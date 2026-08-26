<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10A Dawn 发布包评估

## 结论

Dawn `v20260720.160313` 的 API 头与 Emdawnwebgpu 可以作为匹配原型基线，但官方 Windows Release
包不适合作为 Granit 的通用桌面运行时依赖。自建共享库验证确认 `wgpuCreateInstance` 可以动态
加载，但逐符号加载会把 Dawn 的非稳定 ABI 变成核心运行时边界。桌面正式方向因此调整为 Granit
自有后端插件内部静态链接 Dawn；共享包工作流保留用于升级评估和诊断，不作为最终 SDK 依赖。

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

1. 在锁定的构建镜像中从同一 Dawn 修订构建 monolithic static library。
2. 同时产出匹配的 headers、运行库、符号清单、许可证和 SHA-256 清单。
3. 使用与 Dawn 相同的工具链构建 Granit WebGPU 插件，并只向核心暴露 Granit 自有插件 ABI。
4. 完成插件握手、Instance/Adapter/Device 和离屏回读测试后再接受 ADR-004。

仓库已增加 `Dawn Dependency Packages` 工作流。2026-08-26 的共享库可行性验证已在 Windows 与
Linux 成功。工作流随后调整为从锁定修订构建 monolithic static library，并使用同一工具链构建
`granit_backend_webgpu`、动态加载插件及创建/销毁真实 Dawn Instance；该静态插件链等待首次手动
运行验证。重型工作流仅手动触发，日常特性分支提交和 Pull Request 不重复构建 Dawn。工作流默认
只上传短期验证 Artifact；显式选择发布 SDK 后，Windows 与 Linux 全部通过才会创建版本化预发布
版本，并附带平台压缩包及 SHA-256 清单。Linux 构建镜像后续只消费该 SDK，不作为 Windows 分发
方式。
