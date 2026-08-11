<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-09：Bindless Resource Table 边界

## 元数据

- 设计状态：草案
- 实现状态：未开始
- 路线图任务：D-09
- 优先级：P2
- 前置依赖：D-03、R-08、H-02E
- 后续依赖：H-03～H-05 的大规模资源路径

## 定位

Bindless 是 Renderer 的可选高级能力。Renderer 负责资源表、索引分配、更新、延迟回收、设备能力
检测和 GPU 生命周期同步；Material、Scene 等高层模块只消费稳定索引，不接触 Vulkan Descriptor
Set、Descriptor Pool 或扩展类型。

首版 Bind Group 路径继续作为默认且完整的兼容路径。Bindless 不替换现有接口，不成为创建
Renderer、材质或 Pipeline 的必要条件，也不改变不支持该能力的设备行为。

## 首轮需要确认的边界

- 支持的资源种类：优先评估 sampled texture 与 sampler，再按数据证明扩展 storage 资源。
- 索引模型：零值语义、类型隔离、generation 校验、容量上限和 Shader 中的编码形式。
- 所有权：资源表引用是否阻止资源销毁，以及索引释放后的 GPU 安全复用时机。
- 更新模型：立即更新、帧级批量提交或不可变表；不得让逐资源更新形成高频动态库调用。
- Pipeline 兼容：传统 Bind Group 与 Bindless Shader 变体如何显式区分，禁止静默切换布局。
- 能力查询：使用后端无关的 Granit feature/capacity 描述，不向公共 API 泄漏 Vulkan feature 名称。
- 降级策略：不支持或容量不足时返回明确结果，由上层选择传统 Bind Group 变体。
- 线程安全：分配、更新、释放和命令录制的并发边界及锁竞争基线。

## 与 H-02 的关系

H-02E 的首版版本化材质包只要求传统 Bind Group，避免在包格式尚未稳定时同时引入两套布局。
但包格式必须允许显式记录绑定模型和所需 Renderer 能力，使未来 Bindless 变体可以新增而不歧义地
复用传统变体。

材质系统不得自行持有 Vulkan descriptor 数组。未来启用 Bindless 时，它只把 Renderer 分配的
资源索引写入材质参数，并根据包内要求选择对应 Shader/Pipeline 变体。

## 建议实施顺序

1. **D-09A**：在 H-02E 中预留版本化的绑定模型与 feature requirement 字段，但不实现 Bindless。
2. **D-09B**：用大量 Texture/Sampler 的真实场景测量 Bind Group 的 CPU、内存和更新成本。
3. **D-09C**：确定句柄、索引、容量、并发和延迟复用规则，完成纯 CPU Resource Table 原型。
4. **D-09D**：实现 Vulkan Descriptor Indexing 后端和能力查询，保留传统 Bind Group 回退路径。
5. **D-09E**：接入材质变体，增加生命周期、容量耗尽、并发和性能测试。

## 开始实现的条件

至少满足以下条件后再进入 D-09C：

- H-02E 材质包、变体查找与 Pipeline 缓存已经完成。
- 存在能稳定复现的大量材质或 Texture 场景，而不是仅凭理论复杂度引入功能。
- 测量表明 Bind Group 创建、更新、切换或 Descriptor Pool 占用已成为显著瓶颈。
- 已明确首批目标设备的能力覆盖率和可接受的非 Bindless 回退行为。

## 首版不做

- 不公开 Vulkan descriptor index、set/binding 规则或原生句柄。
- 不把 Descriptor Buffer、Ray Tracing 或 Mesh Shader 与首个 Bindless 原型捆绑实现。
- 不承诺索引可持久化、跨 Renderer 使用或在资源释放后保持有效。
- 不因设备支持 Bindless 就默认启用；启用必须由明确配置和材质变体共同决定。

## 验收标准

- 不支持 Bindless 的设备仍能完整使用传统 Bind Group 路径。
- 旧索引在资源释放后不能错误访问复用槽位中的新资源。
- 资源销毁、索引回收和 GPU 在途使用之间具有可验证的安全边界。
- 公共接口和材质包不包含 Vulkan 类型或扩展常量。
- 性能测试能证明目标场景相对传统 Bind Group 路径存在实际收益。
