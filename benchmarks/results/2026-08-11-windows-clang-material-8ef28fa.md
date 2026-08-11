<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 材质系统首份性能基线

## 环境

- 日期：2026-08-11
- Granit：`8ef28fa`
- 系统：Windows AMD64
- CPU：Intel64 Family 6 Model 158 Stepping 10
- 编译器：Clang 22.1.8
- 构建：Release、共享核心库、静态材质模块
- 变体数量：64
- 每样本迭代：100,000 次
- 预热：5 次
- 样本：30 次

## 原始摘要

| 用例 | 平均值 | P50 | P95 | P99 |
| --- | ---: | ---: | ---: | ---: |
| `parameter_set` | 11.337 ns | 11.272 ns | 11.652 ns | 11.664 ns |
| `variant_lookup` | 44.939 ns | 43.015 ns | 52.309 ns | 66.392 ns |
| `instance_migration` | 181.411 ns | 178.253 ns | 188.300 ns | 189.008 ns |
| `pipeline_cache_hit` | 21.179 ns | 21.120 ns | 21.388 ns | 21.391 ns |

`parameter_set` 每次写入变化的 16 字节 `float4` 并更新 dirty 范围；`variant_lookup` 每次计算一个
feature key 并在 64 个变体中查询；`instance_migration` 每次创建新实例、迁移一个参数并生成报告，
包含堆分配成本。`pipeline_cache_hit` 在计时前已经创建 Pipeline，计时部分只覆盖互斥、缓存键比较
和句柄返回，不包含 Shader/Pipeline 首次创建或 GPU 执行。

## 判断

- 当前参数更新和 Pipeline 缓存命中均约 20 ns 以内，不需要增加无锁缓存或绕过类型检查。
- 64 变体查找的 P50 为 43 ns，当前排序数组查找足够；尚无依据引入哈希表及其额外内存。
- 实例迁移包含分配仍低于 0.2 µs。热替换不是逐 Draw 高频路径，暂不引入对象池。
- 本结果只用于同机器、同构建参数的回归对比，不代表跨机器性能承诺，也不能替代真实帧分析。

## 复测条件

- 参数存储、dirty 合并、变体键算法或查找容器发生变化。
- Pipeline 缓存锁粒度、并发创建策略或键字段发生变化。
- 材质实例迁移增加 Texture/Sampler 所有权验证或批量迁移接口。
- 同环境 P50 或 P95 相对本基线稳定退化超过 10%。
- H-03～H-05 建立真实重复帧后，补充缓存命中率和每帧材质切换分布，而不只测命中耗时。
