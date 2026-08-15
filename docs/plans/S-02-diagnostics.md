<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-02：统一诊断、GPU 调试名称与 Device Lost 报告

## 状态

- 设计状态：已确认
- 实现状态：实现中
- 路线图任务：S-02
- 优先级：P1
- 前置依赖：V-01、F-07、S-01

## 背景与目标

当前 Vulkan Validation Message 和 Granit 生命周期诊断都直接写入标准错误流；Device Lost 已有
Renderer 级粘滞门禁，但只向调用者返回结果码。外部引擎、编辑器和测试框架无法统一接收这些信息，
也无法为 GPU 资源设置能被 RenderDoc、验证层或驱动工具识别的名称。

S-02 建立一个不暴露 Vulkan、适合动态库边界的统一诊断通道，并补齐以下能力：

- 日志等级、类别、文本和用户数据组成的公共 C 回调。
- Vulkan Validation 与 Granit 生命周期诊断走同一个 Renderer sink。
- 为公开资源句柄设置后端调试名称。
- 首次 Device Lost 的操作、结果和后端信息形成一次性报告。

## 非目标

- 不实现完整日志框架、文件滚动、异步队列或格式化库。
- 不在动态库内部启动日志线程。
- 不公开 `VkDebugUtilsMessengerEXT`、`VkObjectType` 或原生 Vulkan 句柄。
- 不保证驱动能够提供设备故障扩展信息；缺少能力时仍返回基础报告。
- 不收集调用栈或自动上传崩溃数据。

## 已确认决策

### 回调与默认输出

- C ABI 使用函数指针、`void* user_data` 和“指针 + 长度”文本，不跨边界传递 STL 或异常。
- 回调在产生诊断的线程同步调用；Vulkan 消息可能来自多个线程，调用方必须保证接收器线程安全。
- 文本只在回调期间有效；Granit 不取得 `user_data` 所有权，其有效期必须覆盖 Renderer。
- 回调不得重入同一 Renderer；内部不得在持有 Registry 全局锁时调用用户代码。
- 未提供回调时继续写标准错误流，保持命令行与现有测试的可见性。
- 用户回调异常不得穿过 C ABI；C++ 包装只接受 `noexcept` 兼容的原生回调。

### 等级与类别

- 等级首版为 `info`、`warning`、`error`。
- 类别首版为 `general`、`validation`、`performance`、`lifecycle` 和 `device`。
- Vulkan severity/type 映射到上述稳定枚举，公共值不复用 Vulkan 数字。

### GPU 调试名称

- 使用 Renderer、公开整数句柄和 UTF-8 名称设置，不要求调用者知道资源的 Vulkan 类型。
- Registry 根据句柄类型和所属 Renderer 查找内部对象；错误类型、跨 Renderer 和失效句柄必须拒绝。
- 名称由 Granit 在调用期间复制或立即提交，调用返回后不保留输入字符串。
- 后端不支持 Debug Utils 时返回 `GRANIT_ERROR_UNSUPPORTED`，不静默报告成功。

### Device Lost

- 每个 Renderer 只记录并输出第一次 Device Lost，后续调用继续稳定返回
  `GRANIT_ERROR_DEVICE_LOST`，但不重复刷屏。
- 基础报告包含触发操作、Granit 结果、是否启用验证和 Renderer domain。
- 若后续引入设备故障扩展，扩展数据追加在内部报告中，不改变基础回调 ABI。

## 实施顺序

1. S-02A（已完成）：增加内部统一 diagnostic sink、等级/类别映射和无回调时的标准错误流回退；
   Vulkan Validation 与生命周期诊断已经接入。
2. S-02B（已完成）：Renderer V4 描述已增加日志回调与 `user_data`，C++20 包装和真实生命周期
   消息回归已经覆盖。
3. S-02C（已完成）：增加通用公开句柄调试名称入口，并接入 Vulkan Debug Utils；纯 CPU
   管理句柄明确返回不支持，失效和跨 Renderer 句柄返回无效句柄。
4. S-02D：让 Device Lost 状态记录首个触发操作并通过 `device` 类别输出一次性报告。
5. S-02E：补齐 C11/C++20、并发回调、重入约束、资源失效和无扩展降级测试及参考文档。

## 测试与验收

- 未设置回调时，验证层和生命周期警告仍可在标准错误流观察到。
- 设置回调后，消息携带稳定等级、类别、准确长度和原始 `user_data`。
- 生命周期回调发生在 Registry 锁外；测试中的回调可以安全写入调用方自己的同步容器。
- 多线程 Validation Message 不破坏消息边界，不依赖 Granit 全局互斥串行化用户回调。
- 合法资源名称可在支持 Debug Utils 的环境设置；错误句柄、跨 Renderer 和失效句柄有稳定结果。
- Device Lost 只报告首次原因，后续门禁不重复发出报告。
- 公共头仍可由 C11 独立包含，且不包含 Vulkan 类型。

## 风险与未决问题

- Vulkan 回调可能在 Renderer 初始化完成前发生，因此 sink 必须先于 Instance 构造并覆盖其析构。
- 用户回调执行时间会直接阻塞产生消息的线程；文档必须建议只做有界复制或入队。
- GPU 名称是否需要被 Granit 长期保存，等待编辑器查询需求出现后再评估；首版只提交给后端。
- 正式 ABI 快照前仍可调整枚举和 Renderer 描述字段，但所有变化必须经过 S-01 回归。
