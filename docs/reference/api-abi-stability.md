<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# API/ABI 稳定候选清单

本文是 v0.50.0 S-86A 的当前事实清单，不代表已经冻结 ABI。公共接口仍以最新安装 SDK 为准；
稳定候选只表示已经具备进入兼容门禁的条件，后续仍需通过布局、符号、Consumer 和生命周期验收。

## Component 分级

| Component | 公共入口 | 当前等级 | v0.50.0 判断 |
|---|---|---|---|
| Core | `granit/core/*.h`、`*.hpp` | 稳定候选 | 结果码、句柄、诊断、版本和基础类型进入布局/符号门禁 |
| Renderer | `granit/renderer/*.h`、`*.hpp` | 稳定候选 | 资源句柄、描述结构、异步操作和生命周期进入负向测试 |
| Window | `granit/window.h`、`window.hpp` | 实验性候选 | 平台生命周期和输入契约继续验证，不冻结平台扩展字段 |
| RenderPipeline | `granit/pipeline/*.h`、`*.hpp` | 实验性 | PBR、Scene、Canvas、Text 和 Debug Draw 仍允许随渲染需求调整 |
| AssetTools | `granit/asset_tools/*.h`、`*.hpp` | 实验性 | 构建器和资产格式与运行时 ABI 分离，暂不承诺长期兼容 |
| Math | `granit/math/*.h`、`*.hpp` | 稳定候选 | 仅保留跨 ABI 值类型和无状态函数，不引入运行时所有权 |
| Integration | `granit/integrations/*` | 不稳定 | SDL3/ImGui 是独立集成入口，不进入核心稳定 ABI |

## 稳定候选共同约束

- C 头文件必须能由 C11 独立包含；C ABI 不出现 STL、异常、模板、虚函数或 Vulkan 类型；
- C++ 包装只提供轻量 RAII 和强类型语义，不维护与 C API 平行的资源状态；
- 公开结构体使用 `struct_size`，保留字段必须置零，当前 0.x 不保留旧布局兼容分支；
- 资源使用 64 位句柄，零值无效；实现必须校验类型、generation 和所属 Renderer；
- 所有异步操作必须定义状态、结果读取时机、取消语义、销毁行为和 Renderer 关闭行为；
- 公共头文件不得包含 Vulkan SDK，安装目标不得向下游传播 Vulkan include/link 依赖。

## 当前不承诺稳定的内容

- `RenderPipeline` 的材质、Scene、Canvas、Text 和 Debug Draw 组织方式；
- `AssetTools` 的构建器参数、资产格式扩展和工具链版本；
- SDL3/ImGui 等第三方集成的宿主对象和平台字段；
- Vulkan 原生互操作、厂商扩展、Runtime Bundle 内部文件布局；
- Gneiss 的 SVT、纹理驻留、预算、VFS 和任务调度。

## S-86 后续证据

本清单将在 S-86B 更新为布局/符号快照，并链接 C11/C++20 Consumer、跨 Renderer、generation、
重复销毁和 Renderer 级联清理测试。只有证据完整的 component 才能进入稳定候选，不以版本号自动
推导 ABI 稳定性。
