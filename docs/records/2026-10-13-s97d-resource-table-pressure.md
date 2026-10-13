<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-97D：Resource Table 分配与更新压力首轮记录

## 日期与范围

- 日期：2026-10-13
- 计划：[S-97](../plans/S-97-0.61.0-resource-table-shader-and-multimaterial-scene.md)
- 环境：Windows Clang Debug、Vulkan 后端、当前默认设备
- 范围：Tutorial 12 的 64 个 Resource Table 创建和 64 次 sampler 更新

## 验证结果

压力模式改为单帧 smoke 生命周期，初始化完成后自动退出，并输出固定格式的计时结果。
同一构建连续运行 3 次：

| 运行 | Resource Table 数量 | sampler 更新数 | 初始化耗时 |
|---:|---:|---:|---:|
| 1 | 64 | 64 | 1014 μs |
| 2 | 64 | 64 | 1169 μs |
| 3 | 64 | 64 | 981 μs |

三次运行均正常退出，没有报告资源表创建、更新或 Vulkan 后端错误。完整 Windows Clang
回归测试 113/113 通过。

同日将 Tutorial 12 的纹理资产改为目录级预加载后，Emscripten Debug 的
`granit_tutorial_12_resource_table_scene` 目标成功链接；原先逐文件预加载会在 Windows
链接阶段触发“命令行太长”。

## 命令

```powershell
cmake --build --preset windows-clang-debug --target granit_tutorial_12_resource_table_scene -j 4
build/windows-clang-debug/bin/granit_tutorial_12_resource_table_scene.exe --stress-test
```

## 限制与后续

本记录测量的是 CPU 侧 Resource Table 创建和 sampler 更新压力，不是 Bind Group 对照、命令
录制成本或 GPU 时间；64 个压力表也不等价于 64 个实际材质 Draw。S-97D 仍需增加同规模
Bind Group 对照，并在可用设备上补充 GPU 时间和 Descriptor 写入统计，之后才能判断是否有
性能收益。
