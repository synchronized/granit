<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-91B：Bindless 跨平台能力矩阵

## 日期与范围

- 日期：2026-10-08
- 计划：[S-91](../plans/S-91-0.55.0-bindless-pressure-and-boundary.md)
- Linux workflow：[37671553111](https://github.com/synchronized/granit/actions/runs/37671553111)
- Windows workflow：[37669814093](https://github.com/synchronized/granit/actions/runs/37669814093)
- Emscripten workflow：[37669811761](https://github.com/synchronized/granit/actions/runs/37669811761)

## 结果

| 平台 | 结果 | 证据边界 |
|---|---|---|
| Windows Clang 本地 Vulkan 硬件 | Bindless 专项通过，2419 断言 | 含 GPU timestamp、readback、压力和容量耗尽 |
| Linux Clang shared/static CI | workflow 成功；Bindless 专项明确 skip | 容器没有 `glslc`，未生成实验 Shader，不能写成 Linux GPU 通过 |
| Linux Vulkan model-viewer smoke | 通过 | 证明 Linux Vulkan/Validation 集成路径可运行，但不是 Bindless 专项 |
| Windows CI shared/static | 通过 | 托管 runner 没有 Vulkan ICD，按工作流约定排除 GPU 测试 |
| Emscripten browser/model-viewer/imgui | 通过 | 继续使用 WebGPU/Bind Group，不引入 Vulkan Bindless |

## 结论

- 不支持实验 Shader 工具链或 Vulkan Bindless 能力时，测试明确跳过/回退，不创建公共 Bindless ABI；
- Linux 本轮的限制是 `glslc` 工具链缺失，而不是把 skip 当作 Lavapipe 性能结果；
- 传统 Bind Group 仍是跨平台默认路径，WebGPU/Emscripten 不模拟 Vulkan Bindless；
- Linux 专项硬件/软件 Vulkan Bindless 复测保留为后续 CI 工具链增强项，不阻塞本版本发布的内部实验结论。
