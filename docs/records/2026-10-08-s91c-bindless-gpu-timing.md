<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-91C：Bindless 与传统路径 GPU 时间戳基线

## 日期与范围

- 日期：2026-10-08
- 计划：[S-91](../plans/S-91-0.55.0-bindless-pressure-and-boundary.md)
- 范围：同一设备、同一源纹理、同一输出图像上的 Bindless/传统各一次 compute dispatch

## 验证结果

Windows Clang Debug + Vulkan Validation 下，同一 Vulkan 进程重复运行 4 次，均通过；每次均
完成 Bindless 和传统路径的真实队列提交、GPU timestamp 读取以及原有 readback 校验。

| 运行 | Bindless dispatch | 传统 Bind Group dispatch | 断言 |
|---:|---:|---:|---:|
| 1 | 13.416 μs | 14.167 μs | 2417 |
| 2 | 14.833 μs | 14.000 μs | 2417 |
| 3 | 14.083 μs | 14.250 μs | 2417 |
| 4 | 13.166 μs | 14.167 μs | 2417 |

## 已验证契约

- 两条路径使用各自的 Vulkan pipeline/layout，并在相同设备队列上提交实际 dispatch；
- 时间戳查询池将设备 tick 换算为纳秒，结果顺序和非负时长均通过检查；
- Vulkan Validation 未报告 descriptor、pipeline layout、同步、query 或资源生命周期错误；
- 原有输出图像 readback 仍为绿色像素，说明 GPU 写入路径未被计时插桩破坏；
- 本机设备上，单 dispatch 的 GPU 时间处于相同数量级，不能据此宣称 Bindless 有稳定 GPU 收益；
- 512 槽位资源表填满后，继续注册 texture/sampler 均返回 `GRANIT_ERROR_OUT_OF_MEMORY`。

## 限制与结论

本轮仍使用单个 1×1 纹理、单个 sampler、单个 workgroup，且不同逻辑槽位复用相同底层资源。
它补齐了真实 queue/GPU timestamp 证据，但还不是多材质、多纹理内容或高占用场景的端到端结论。
Linux 的实验 Shader 工具链边界和不支持设备回退已记录；更大真实资源占用数据仍不是本版本结论。

## 验证命令

```text
cmake --build build/windows-clang-debug --target granit_vulkan_backend_test -j 4
$env:GRANIT_VULKAN_VALIDATION="on"
build/windows-clang-debug/bin/granit_vulkan_backend_test.exe "[bindless]" --reporter compact
ctest --test-dir build/windows-clang-debug -R 'granit\.backend\.vulkan|granit\.renderer\.resources' --output-on-failure
```
