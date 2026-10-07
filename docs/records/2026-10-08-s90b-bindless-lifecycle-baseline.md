<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-90B：Bindless 生命周期与传统 Bind Group 基线

## 日期与范围

- 日期：2026-10-08
- 计划：[S-90](../plans/S-90-0.54.0-vulkan-bindless-admission.md)
- 范围：索引槽位回收、GPU 完成点、传统/Bindless descriptor 更新和显式 Pipeline 变体

## 已验证契约

- texture 和 sampler 使用各自的 CPU Resource Table，二者可以共享相同的逻辑索引；
- Resource Table 的 0 号索引保留为无效值，Vulkan descriptor 数组额外预留 0 号槽位；
- 提交使用 texture/sampler 槽位后立即释放，`collect(0)` 不复用旧槽位；
- GPU fence 完成后执行 `collect(1)`，旧槽位才允许重新分配；
- Bindless Shader/Pipeline 使用独立 Descriptor Set Layout，传统 Shader/Pipeline 使用独立布局，
  测试中显式创建两者，禁止根据设备能力静默替换布局。

## A/B 基线

在同一个 Vulkan 进程、同一组 texture/sampler、同一批 256 次 descriptor 更新下，Windows
Clang Debug + Vulkan Validation 运行结果如下：

| 路径 | 运行 1 | 运行 2 | 运行 3 | 运行 4 | 运行 5 |
|---|---:|---:|---:|---:|---:|
| Bindless descriptor 更新 | 7300 ns | 7500 ns | 7300 ns | 7500 ns | 7300 ns |
| 传统 Bind Group 更新 | 7100 ns | 7200 ns | 7200 ns | 7300 ns | 7200 ns |

该微型负载没有显示 Bindless 更新收益，且传统路径略快。它只能作为 descriptor 更新基线，
不能推导大规模材质场景的端到端结论；因此 Bind Group 继续是默认路径。

## 验证命令

```text
cmake --build build/windows-clang-debug --target granit_vulkan_backend_test -j 4
$env:GRANIT_VULKAN_VALIDATION="on"
build/windows-clang-debug/bin/granit_vulkan_backend_test.exe "[bindless]" --reporter compact
```

结果：通过 62 个断言，未报告 Vulkan Validation Error；另有传统 `granit.backend.vulkan`
测试保持通过。

## 当前结论

S-90B 的内部生命周期和显式变体准入条件已满足。S-90C 仍需在真实多材质/多纹理工作负载上
补充绑定切换、录制、GPU 时间和 descriptor/内存占用测量，不能把本记录的微型更新基线当作
Bindless 性能收益证明。
