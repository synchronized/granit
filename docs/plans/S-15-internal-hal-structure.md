<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-15：私有 HAL 结构整理

## 状态

- 实现状态：已确认；S-15A 实现中
- 前置依赖：S-10、S-12、S-13
- 优先级：P1

## 背景与目标

Granit 已使用 `src/backend` 中的 `backend_*` 契约隔离 Registry 与 Vulkan/WebGPU 原生实现，
但当前能力接口、插件传输 ABI 和具体后端实现位于同一目录层级。Registry 还会在多个操作中重复
使用 `dynamic_pointer_cast` 发现能力，使依赖关系难以阅读，也把错误推迟到具体调用路径。

本任务整理现有私有 HAL，不创建第二套抽象。目标包括：

- 在 Renderer 注册时一次性发现并保存后端能力接口，集中表达必需能力与可选能力。
- 让 Registry 从明确的能力集合取得资源、命令、Queue、呈现和诊断接口。
- 区分 HAL 契约、插件 ABI/Loader、Vulkan 实现和 WebGPU Adapter 的目录职责。
- 保持公共 C/C++ API、句柄语义、命令批量粒度和运行时行为不变。

## 非目标

- 不公开 HAL，不为其提供 ABI 稳定承诺。
- 不修改公共 Renderer、资源、Pipeline 或命令接口。
- 不强制 Vulkan 与 WebGPU 支持完全相同的可选能力。
- 不移除桌面 WebGPU 动态 Provider 或 Emscripten 静态 Provider。
- 不把每条 Draw、Dispatch 或资源访问改成独立虚函数或插件调用。

## 已确认决策

- `backend_*` 是唯一私有 HAL 契约，不新增平行的 `hal_*` 接口。
- Registry 继续是公共句柄、所有权、父子关系、线程安全和公共参数校验的唯一实现。
- 能力集合在 Renderer 注册时形成不可变快照。根 Renderer、资源、命令等必需接口缺失时注册失败；
  Pipeline Cache、时间戳和调试名称等可选接口允许为空，并返回既有“不支持”结果。
- 能力集合持有 `shared_ptr`，保证异步命令、延迟回收和资源记录的生命周期不短于后端状态。
- `plugin_api.h` 仅表示跨动态库边界的 C 传输 ABI；WebGPU Adapter 负责将其转换为 HAL 契约。
- 目录迁移只改变内部包含路径和构建源集，不改变安装内容或目标名称。

## 实施顺序

### S-15A：能力接口聚合

1. 增加内部 `backend_interfaces`，集中保存根状态和各职责接口。
2. Renderer 注册时一次性发现接口并校验所有后端都必须具备的最小集合。
3. Registry 根表保存聚合对象，逐步移除各编译单元重复的 `dynamic_pointer_cast`。
4. 为完整、缺失必需能力和缺失可选能力增加测试替身。

### S-15B：Registry 依赖收敛

1. 资源记录只保存实际使用的能力接口，不重复执行能力发现。
2. 命令、呈现、Shader 和 Pipeline 路径统一从聚合对象取得依赖。
3. 增加静态边界检查，阻止 Registry 引入 Vulkan、WebGPU 或插件原生类型。

### S-15C：目录职责整理

将现有内部文件渐进整理为：

```text
src/backend/
├─ contracts/  # backend_* HAL 接口、描述和值类型
├─ plugin/     # 插件 C ABI、动态库 Loader 与传输桥
├─ vulkan/     # Vulkan HAL 实现
└─ webgpu/     # WebGPU HAL Adapter 与 Provider 实现
```

先更新构建源集和内部 include，再删除旧路径；不同时进行接口语义重写。

### S-15D：文档与最终验收

1. 更新架构 Concept，给出调用方向、必需/可选能力和插件边界。
2. 保持 ADR-003 的既有决策不变；若实施发现需要改变决策，再新增替代 ADR。
3. 记录跨后端构建、测试、安装 Consumer、浏览器和模型查看器验收结果。

## 测试与验收

- Windows/Linux 的共享与静态 Vulkan 构建及完整测试通过。
- 桌面 Dawn WebGPU 和 Emscripten WebGPU 集成测试通过。
- Vulkan、桌面 WebGPU 和浏览器模型查看器参考截图保持在既有阈值内。
- 测试替身证明缺失必需能力会在注册阶段失败，缺失可选能力仍可创建 Renderer。
- `src/renderer` 不包含 Vulkan、WebGPU、Dawn 或平台插件原生类型。
- 公共头文件、导出符号、安装清单和 C ABI 快照不发生变化。

## 风险与未决问题

- 一次性迁移全部 Registry 调用点会产生较大机械改动，因此按能力域分批提交并持续运行回归。
- 聚合对象若同时保存根状态和多个别名 `shared_ptr` 会增加引用计数，但每个 Renderer 只创建一次，
  不进入每帧热点；最终仍需通过现有性能基线确认。
- 目录重命名会影响较多内部 include，必须与语义重构分开提交，以便审查和回退。
