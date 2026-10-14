<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-97D：Resource Table 与 Bind Group A/B 压力对照首轮记录

## 日期与范围

- 日期：2026-10-14
- 计划：[S-97](../plans/S-97-0.61.0-resource-table-shader-and-multimaterial-scene.md)
- 环境：Windows Clang Debug、Vulkan 后端、当前默认设备
- 负载：Tutorial 12，32 个材质、32 个 Draw

## 验证方式

Tutorial 12 增加两个压力入口：

```powershell
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --stress-test
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --bind-group-stress-test
```

两条路径使用同一份模型、纹理和绘制负载。Resource Table 路径额外创建 64 个压力表并执行
sampler 更新；Bind Group 路径额外创建 64 个 Bind Group，每个包含纹理和 sampler 两个条目，
使用传统 Material/Bind Group implementation。
两条路径均启用已有 RenderPipeline GPU timestamp 指标，输出场景初始化 CPU 时间和 Shadow、Opaque、
Tone Mapping、Total GPU 时间。

## 首轮结果

| 路径 | 场景初始化 | Resource Table 更新 | Total GPU |
|---|---:|---:|---:|
| Resource Table | 117178 μs | 82 | 6.500890 ms |
| Bind Group | 139225 μs | 0 | 6.662223 ms |

Resource Table 压力入口另报告 64 个压力表和 64 次 sampler 更新；两条路径均正常退出，GPU 指标
均成功返回。当前结果只是单机、单负载、Debug 构建的首轮观测，不能据此宣布 Resource Table
在所有设备上更快。

## 五轮重复测量

同一 Windows Clang Debug、16 材质/16 Draw 负载下分别运行 5 次，取中位数：

| 路径 | 场景初始化 p50 | Total GPU p50 |
|---|---:|---:|
| Resource Table | 117024 μs | 6.384 ms |
| Bind Group | 115629 μs | 6.486 ms |

在这组样本中 Resource Table 的 GPU p50 约快 1.6%，但 CPU 初始化 p50 约慢 1.2%。差异很小，且
两条路径现在都包含 64 个额外的压力资源创建，因此这只能作为当前设备上的观测，不能作为跨设备
或产品默认路径的准入依据。

对称压力入口的单次结果如下：

| 路径 | 压力资源数量 | 压力资源创建耗时 | Total GPU |
|---|---:|---:|---:|
| Resource Table | 64 个表、64 次更新 | 1001 μs | 6.377 ms |
| Bind Group | 64 个组、128 个条目 | 941 μs | 6.396 ms |

## 资源压力矩阵首轮

通过 `--stress-test-{4,16,32,64}` 和
`--bind-group-stress-test-{4,16,32,64}` 运行同一负载，改变额外资源创建数量：

| 数量 | Resource Table 创建 | Bind Group 创建 | Resource Table GPU | Bind Group GPU |
|---:|---:|---:|---:|---:|
| 4 | 82 μs | 87 μs | 7.877 ms | 6.371 ms |
| 16 | 309 μs | 472 μs | 6.547 ms | 6.528 ms |
| 32 | 486 μs | 580 μs | 6.423 ms | 6.523 ms |
| 64 | 1076 μs | 1003 μs | 6.387 ms | 6.613 ms |

矩阵中的 GPU 数据每档只采集一轮，4 档尤其受到单帧噪声影响；创建耗时随数量增长的趋势更有
参考价值，但仍需多轮 p50/p95、Release 构建和跨平台验证。这里的数量是额外资源对象压力，不能
等同于场景材质数或 Draw 数。

## 真实 32 材质场景首轮

Tutorial 12 的权威资产已由仓库内生成器重新生成：

```powershell
blender --background --python tools/generate_resource_table_gallery.py -- `
  --count 32 examples/assets/tutorials/12_resource_table_scene
```

该资产包含 32 个对象、32 个材质、32 张 BaseColor 纹理和 32 张
MetallicRoughness 纹理。两条路径在同一资产上各运行一次：

```powershell
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --stress-test-32
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --bind-group-stress-test-32
```

| 路径 | 材质/Draw | 额外压力资源 | 场景初始化 | Total GPU |
|---|---:|---:|---:|---:|
| Resource Table | 32 / 32 | 32 个表、66 次资源表更新 | 185338 μs | 6.615390 ms |
| Bind Group | 32 / 32 | 32 个组、64 个条目 | 182193 μs | 6.618390 ms |

两条路径均正常退出，且确认运行时实际读取了 32 材质场景。该结果仍是单机、Debug、单轮样本；
其中额外压力资源叠加在真实 32 材质场景之上，不能单独解释为 Resource Table 的产品级收益。

随后在同一环境下各重复运行 5 次，取中位数：

| 路径 | 场景初始化 p50 | Total GPU p50 |
|---|---:|---:|
| Resource Table | 177610 μs | 6.728 ms |
| Bind Group | 187022 μs | 6.747 ms |

该样本中 Resource Table 的 CPU 初始化 p50 低约 5.0%，GPU 总时间低约 0.3%；差异仍处于单机
Debug 测量范围，不能替代 Release、跨驱动和跨平台数据。

## Windows Clang Release 收口

Release 构建最初可以完成链接，但 CTest 启动 Tutorial 12 时报告 `0xc0000135`。原因是 FetchContent
构建的 `SDL3.dll` 留在 `_deps/granit_sdl3-build`，没有进入 Granit 统一的 `bin` 运行目录。已在
`granit_window` 的 POST_BUILD 部署步骤中补齐复制，避免 Debug 构建目录已有残留 DLL 掩盖问题。

修复后执行：

```powershell
cmake --preset windows-clang-release `
  -DGRANIT_SHADER_TOOLCHAIN_ROOT=<vendor-cache-toolchain> `
  -DGRANIT_DXC_EXECUTABLE=<toolchain-bin-dxc> `
  -DGRANIT_TINT_EXECUTABLE=<toolchain-bin-tint>
cmake --build --preset windows-clang-release --target granit_tutorial_12_resource_table_scene
ctest --preset windows-clang-release --output-on-failure `
  -R "granit\.tutorial\.12_resource_table_scene|granit\.documentation\.links"
```

结果为 CTest 2/2 通过；Release 单轮压力结果为 Resource Table CPU 22986 μs、GPU 6.608 ms，
Bind Group CPU 51009 μs、GPU 6.630 ms。该数据仅证明 Release 运行和部署正常，不作为跨设备性能结论。

随后完整构建 Release 测试目标并执行全套 CTest，结果为 **113/113 通过**，覆盖教程、Model Viewer
核心、AssetTools、ABI、Vulkan 后端、RenderDoc 桥接、GPU smoke 和打包门禁。

## 限制与后续

## Emscripten 与 Chrome WebGPU 回归

在同一 Windows 主机使用 `emscripten-debug` 构建 Tutorial 02、09、10、12，并使用仓库内
Playwright/Chrome 脚本验证：

| 教程 | 结果 |
|---|---|
| Tutorial 02 | 多帧、Canvas、统一输入、Resize 通过 |
| Tutorial 09 | 多帧、特性状态、Resize 通过 |
| Tutorial 10 | 多帧、特性状态、Resize 通过 |
| Tutorial 12 | 多帧、特性状态、Resize 通过 |

四个教程均未报告 WebGPU validation error 或初始化诊断错误。该结果证明 WebGPU Bind Group 回退
和近期 Storage Buffer/PBR 归档修复在当前 Chrome 环境可用；不替代 Linux Lavapipe 与其它浏览器
驱动验证。

- 当前仍尚未接入后端级创建/Descriptor 写入计数器；压力计时覆盖公共 API 创建路径，不等同于底层
  驱动 Descriptor 写入耗时；
- 尚未完成真实 4/16/32 材质和 8/32/64 Draw 的多轮矩阵化压力报告；
- 还需要 Linux Lavapipe、Release 构建和 WebGPU Bind Group 回退数据；
- GPU timestamp 不可用时只保留 CPU 和资源统计，并明确标记 GPU 数据 unavailable。

## Linux Actions 首轮失败与修复

PR #137 的 `lavapipe-resource-table` 作业已确认能够安装并选择
`/usr/share/vulkan/icd.d/lvp_icd.json`，但首次冷构建在链接 `pbr_resource_table.grshlib` 时失败。
根因是 Shader Object 缓存层只把 `portable` Vulkan variant 计入 SPIR-V sidecar 判定；
`vulkan_native` variant 写入后错误删除 `.spv`，导致 Library Builder 找不到 Vulkan 载荷。

已修复为 portable/native 任一 Vulkan variant 都保留 SPIR-V sidecar，并补充 native sidecar 回归测试。
修复后的 PR 运行结果尚未取得，因此 Linux Lavapipe 仍未计入版本通过证据。
