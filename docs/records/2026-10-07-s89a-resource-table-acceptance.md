<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-89A：CPU Resource Table 验收记录

## 范围

本阶段只验证 Bindless 的 CPU 侧索引契约，不实现 Vulkan Descriptor Indexing，也不改变公共 ABI
和默认 Bind Group 路径。

## 已验证

- 资源索引包含 generation，槽位回收后旧索引不能解析到新资源；
- 解析时校验资源类型和 Renderer 归属；
- 资源释放后，在途完成点到达前不能复用槽位；
- 重复释放、无效索引和容量耗尽返回明确失败结果；
- Windows Clang Debug 的 `granit.renderer.resources` 测试通过。

## 实现边界

原型位于内部 `src/renderer/resource_table.hpp`，不进入安装文件集，不包含 Vulkan 类型，不承诺
跨 Renderer 或跨进程持久化。后续 S-89B 需要把同一套语义接到后端无关能力探针。
