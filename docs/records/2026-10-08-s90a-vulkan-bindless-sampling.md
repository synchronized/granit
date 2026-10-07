<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-90A：Vulkan Bindless sampled texture/sampler 验收记录

## 日期与范围

- 日期：2026-10-08
- 计划：[S-90：0.54.0 Vulkan Bindless 准入验证](../plans/S-90-0.54.0-vulkan-bindless-admission.md)
- 范围：内部 Vulkan Descriptor Indexing 表、运行时数组索引、storage image 回读

## 已实施

- 新增内部 `vulkan_bindless_descriptor_table`，使用 sampled image 和 sampler 两个 descriptor
  数组，支持部分绑定和 sampler 可变数量；不进入公共头文件或安装 SDK。
- 新增 Vulkan-only compute Shader，按运行时数组索引读取第 1 个 texture/sampler 槽位，写入
  storage image，再通过 GPU 回读校验结果。
- 设备能力不足时测试明确跳过，普通 Renderer 和 WebGPU 路径不依赖该实验组件。

## 验证命令

```text
cmake --build build/windows-clang-debug --target granit_vulkan_backend_test -j 4
build/windows-clang-debug/bin/granit_vulkan_backend_test.exe "[bindless]" --reporter compact
$env:GRANIT_VULKAN_VALIDATION="on"
build/windows-clang-debug/bin/granit_vulkan_backend_test.exe "[bindless]" --reporter compact
```

结果：两次均通过，第二次包含 46 个断言；验证层未报告错误。另有
`granit.backend.vulkan` 测试通过。

## 当前结论

S-90A 已证明 Granit 当前 Vulkan 设备能力组合可以创建 Descriptor Indexing 表、写入真实
texture/sampler descriptor，并执行索引采样。该结果只证明后端内部实验准入，不证明公共
Bindless API、材质变体或性能收益已经成立。

S-90B 仍需完成 Shader/Pipeline 变体显式选择、GPU 完成点关联和索引延迟回收；S-90C 仍需
完成与传统 Bind Group 的可重复 A/B 测量。
