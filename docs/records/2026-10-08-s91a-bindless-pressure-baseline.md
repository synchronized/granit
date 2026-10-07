<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-91A：Bindless Descriptor 压力基线

## 日期与范围

- 日期：2026-10-08
- 计划：[S-91](../plans/S-91-0.55.0-bindless-pressure-and-boundary.md)
- 范围：8/64/512 个逻辑 texture/sampler 槽位、批量 Bindless 更新、传统逐槽位更新和延迟回收

## 验证结果

Windows Clang Debug + Vulkan Validation 下，同一 Vulkan 进程重复运行 3 次，均通过 2400 个断言。
每轮使用 16 次压力迭代；Bindless 每次以批量 descriptor writes 更新，传统路径对同一数量的
逻辑槽位逐次更新单一 Bind Group。

| 逻辑槽位 | Bindless 批量更新 | 传统 Bind Group 逐槽位更新 | 运行次数 |
|---:|---:|---:|---:|
| 8 | 2.8～3.0 μs | 3.9～4.5 μs | 3 |
| 64 | 19.7～21.6 μs | 28.7～30.7 μs | 3 |
| 512 | 156.4～171.5 μs | 230.7～247.9 μs | 3 |

## 已验证契约

- 512 容量的独立内部 Descriptor Table 可以创建并完成 8/64/512 槽位更新；
- texture 和 sampler 使用独立 CPU Resource Table，但可以按相同逻辑索引写入 Descriptor 数组；
- 每轮压力结束后释放全部槽位并执行完成点收集，没有发生槽位泄漏或 generation 错误；
- Vulkan Validation 未报告 descriptor、pipeline layout、同步或资源生命周期错误；
- 传统 Bind Group 更新仍可在同一进程完成，未改变默认路径。

## 限制与结论

本轮不同逻辑槽位暂时复用同一底层 1×1 纹理和采样器，测量的是 Descriptor/CPU 绑定压力，
不是多材质内容差异、命令录制或 GPU 时间收益证明。结果支持继续实现 S-91B/S-91C，暂不支持
公开 Bindless ABI 或默认启用 Bindless。

## 验证命令

```text
cmake --build build/windows-clang-debug --target granit_vulkan_backend_test -j 4
$env:GRANIT_VULKAN_VALIDATION="on"
build/windows-clang-debug/bin/granit_vulkan_backend_test.exe "[bindless]" --reporter compact
```
