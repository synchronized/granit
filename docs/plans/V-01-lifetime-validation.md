<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# V-01：资源生命周期验证与诊断

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：V-01
- 优先级：P0
- 前置依赖：R-03、R-05、R-06、R-07
- 后续依赖：R-08、S-02

## 背景

Renderer 当前拥有 Buffer、Texture、Texture View、Sampler、Surface、Swapchain 和借用的
Backbuffer 资源。销毁 Renderer 时会安全地级联清理这些对象，因此即使用户遗漏显式销毁，
Vulkan Validation Layer 通常也不会报告泄漏。

Vulkan 验证层只理解 Vulkan 对象、同步和 Layout，不理解 Granit 的整数句柄、generation、
Renderer domain、公开所有权或借用资源。Granit 需要自己的验证层补充这些语义，但不能因此
破坏异常恢复和级联清理能力。

## 目标

- 验证模式下发现 Renderer 销毁时仍存在的用户拥有资源。
- 验证模式下发现父资源销毁时仍存在的用户拥有子资源。
- 按资源类型输出简洁、可定位且不会刷屏的汇总诊断。
- 区分用户拥有资源、内部临时资源和 Swapchain 借用资源。
- 无论是否发现遗漏，都继续使句柄失效并完成底层清理。
- 为后续日志回调、调试名称、严格模式和延迟销毁诊断建立内部结构。

## 非目标

- 本任务不替代 `VK_LAYER_KHRONOS_validation`。
- 不在第一版增加公开日志回调、资源调试名称或调用栈采集。
- 不因为存在活动子资源而让 `granit_renderer_destroy` 返回失败。
- 不在 Release 默认配置中无条件保存昂贵的创建位置或线程调用栈。
- 不实现 R-08 的 GPU 使用完成判断和延迟销毁队列。

## 验证层职责

### Vulkan Validation Layer

继续负责：

- Vulkan 参数、对象父子关系和扩展使用错误。
- Pipeline barrier、Image Layout 和同步错误。
- 仍被 GPU 使用时销毁 Vulkan 对象。
- Descriptor、Command Buffer 和 Queue 提交规则。

### Granit 生命周期验证

负责：

- Renderer 销毁时仍存活的用户拥有资源。
- Texture 销毁时仍存活的用户 View，以及 Surface 销毁时仍存活的 Swapchain。
- 错误资源类型、旧 generation 和跨 Renderer domain 使用。
- 用户尝试销毁 Swapchain Backbuffer 等借用资源。
- 嵌套映射、映射期间销毁和不合法的资源状态转换。
- 未来延迟销毁队列未排空、资源调试名称和上层所有权诊断。

两层验证同时启用且互补。不能把 Granit 生命周期问题伪装成 Vulkan 对象泄漏，也不能通过
Granit 统计取代 Vulkan 同步验证。

## 销毁策略

Renderer 销毁采用以下固定流程：

```text
冻结新的资源获取
→ 统计仍存活的用户拥有资源
→ 在验证模式输出汇总警告
→ 从 Registry 移除所有公开句柄
→ 按依赖顺序在 Registry 锁外级联清理
→ 完成 Renderer 销毁并返回成功
```

发现活动资源不是阻止清理的理由。返回失败会迫使调用者再次销毁 Renderer，并可能在错误恢复、
Device Lost 或 C++ 析构期间制造真正的泄漏。

未来严格模式可以把诊断升级为断言、测试失败或 error 级日志，但仍必须完成清理。严格模式不应
简单地让销毁函数提前返回。

## 统计范围

第一版统计以下用户拥有资源：

- Buffer。
- 普通 Texture。
- 用户显式创建的 Texture View。
- Sampler。
- Surface。
- Swapchain。
- Command Recorder。

以下对象不作为用户泄漏单独报告：

- Swapchain 拥有的借用 Texture 和默认 View。
- staging Buffer、上传 Fence、Command Pool 等调用内临时对象。
- Renderer 内部的 Instance、Device、Allocator 和 Queue。
- 已进入 R-08 延迟销毁队列且公开句柄已经正常失效的对象。

父资源与子资源同时存活时可以分别计数，但日志应注明级联关系，避免把一个遗漏的 Swapchain
展开成多条 Backbuffer “泄漏”。

## 诊断内容

默认输出单条汇总，例如：

```text
[granit][validation] Renderer 销毁时仍有 5 个用户资源：
Buffer=2, Texture=1, TextureView=1, Sampler=1；将级联释放。
```

每种类型最多列出少量句柄样本，建议默认 8 个；超过部分只报告数量。句柄仅用于本次进程内
诊断，不应被当作持久身份。

第一版每条资源记录至少包含：

- 资源类型和公开句柄。
- 所属 Renderer domain。
- owned、borrowed 或 internal 分类。
- 单调递增的创建序号。

创建线程 ID、用户调试名称、源码位置和调用栈属于 S-02 后续增强，避免当前记录成本过高。

## 启用方式

第一版复用 `GRANIT_RENDERER_ENABLE_VALIDATION_BIT` 和 C++ 的 `enable_validation`：

- 请求并启用 Vulkan Validation Layer 与 debug utils。
- 同时启用 Granit 生命周期验证。
- 缺少 Vulkan 验证环境时仍保持当前行为：Renderer 创建返回
  `GRANIT_ERROR_UNSUPPORTED`，不静默降低验证等级。

后续若需要在没有 Vulkan Layer 的机器上单独启用 Granit 验证，可以追加独立 flag。新增 flag
必须位于现有 ABI 可扩展字段中，不能改变当前标志含义。

## 日志通道

第一版沿用当前 Vulkan debug messenger 使用的标准错误流。输出发生在 Renderer 销毁的调用
线程中。

S-02 再增加统一日志回调，并明确：

- 日志等级与类别。
- 回调线程和调用顺序。
- 字符串只在回调期间有效。
- `user_data` 由调用者拥有。
- 为避免锁重入，回调期间不得再次调用同一 Renderer。

生命周期统计不得直接依赖 `stderr`，内部先形成结构化诊断，再交给当前日志 sink，以便后续替换
为回调而无需改动 Registry。

## 线程与锁

- 统计必须发生在 Renderer 已从公开 Registry 冻结、但子资源尚未移除时。
- 在持有 Registry 锁时只收集定宽元数据，不格式化长字符串、不调用用户回调。
- 日志格式化和输出在释放 Registry 锁后执行。
- 诊断过程中不能延长资源的公开有效期。
- 并发中已经取得内部共享状态的操作可以结束；新操作不能再取得已销毁 Renderer。

## 实施步骤

1. 为 Renderer 保存 Granit 验证是否启用。
2. 为 Registry 资源记录补充 owned/borrowed/internal 分类和创建序号。
3. 在 Renderer 销毁路径冻结新操作并收集活动资源快照。
4. 过滤 Swapchain Backbuffer 等借用资源，按公开资源类型汇总。
5. 在锁外通过现有诊断通道输出一条有上限的警告。
6. 保持现有级联移除句柄和依赖顺序清理流程。
7. 为普通销毁、遗漏资源、纯借用资源和并发边界补充确定性测试。
8. R-08 实现后把延迟销毁队列状态加入验证，但不重复报告正常退役资源。

## 测试矩阵

- 未启用验证时，Renderer 级联清理不输出生命周期警告。
- 启用验证且所有用户资源已显式销毁时不输出警告。
- 遗留 Buffer、Texture、用户 View 和 Sampler 时按类型准确汇总。
- 只剩 Swapchain Backbuffer 借用 Texture/View 时不将其报告为泄漏。
- 遗留 Surface/Swapchain 时报告父资源，但不把其内部 Backbuffer 展开为用户泄漏。
- Texture 级联销毁用户 View、Surface 级联销毁 Swapchain 时输出父子关系诊断。
- 输出后 Renderer 仍销毁成功，旧句柄全部返回无效句柄。
- 大量资源只输出有限样本，日志长度有确定上限。
- Clang 动态库和 MSVC 静态库严格警告构建通过。
- Vulkan Validation Layer 下销毁顺序不产生 Vulkan 生命周期错误。

## 验收标准

- Granit 与 Vulkan 两类验证职责在文档和实现中明确分离。
- 验证模式能够发现用户遗漏的公开资源，而不是依赖 Vulkan 偶然报告。
- 借用和内部资源不会产生误报。
- 诊断不发生在 Registry 锁内，也不改变销毁结果。
- Renderer 始终完成句柄失效和底层级联清理。
- 后续日志回调和 R-08 可以复用结构化快照，不需要重写资源统计。

## 后续任务

- R-08：延迟销毁队列、GPU 完成点和退出排空诊断。
- S-02：统一日志回调、GPU 调试名称、Device Lost 报告和严格验证模式。

## 实现结果

已完成：

- Renderer 内部保存 Granit 验证开关，公开 ABI 未发生变化。
- Registry 为公开资源记录单调递增的创建序号。
- Renderer 销毁时在锁内收集固定容量快照，在锁外输出诊断并继续级联清理。
- 每类资源最多保留 8 个句柄样本；总数始终准确。
- 普通 Texture/View 与 Swapchain 借用 Backbuffer 根据公开销毁属性区分，借用资源不产生误报。
- Texture/View 和 Surface/Swapchain 父子销毁路径复用有界样本格式，只诊断用户拥有子资源。
- 增加快照分类、创建序号和样本上限单元测试。

验证环境：Windows Clang + Ninja Debug 动态库、Visual Studio 2022 Debug 静态库，严格警告
构建和全部测试通过。
