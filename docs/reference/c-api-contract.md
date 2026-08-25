<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 核心 C API 契约

本文统一说明核心 `granit` component 的 C ABI 所有权、错误和扩展规则。各资源的状态约束与具体
失败条件仍以对应参考页为准。

## 句柄与所有权

- 所有公开对象使用 64 位整数句柄，零值无效；句柄不能持久化，也不能跨 Renderer 使用。
- `*_create` 成功后，调用者拥有返回句柄，并负责调用配对的 `*_destroy`。创建失败不产生句柄。
- 销毁后旧句柄立即失效；重复销毁、资源类型错误和跨 Renderer 使用返回
  `GRANIT_ERROR_INVALID_HANDLE`。
- Renderer 是所有 GPU 资源的根对象。调用者应先结束使用并销毁子资源，再销毁 Renderer；验证
  模式会报告遗留的用户资源，随后仍执行级联清理。
- Swapchain backbuffer View 等明确标记的借用句柄由父对象管理，调用者不得单独销毁。

## 指针、字符串与内存

- 未明确说明会保留的输入指针都只在调用期间借用。创建函数会在返回前复制需要长期保存的数据。
- 字符串使用“指针 + 字节长度”，不要求以零结尾；有效范围内通常不允许嵌入零字符。
- 查询接口采用调用者分配的输出内存。容量不足时返回错误或所需大小，Granit 不把内部内存所有权
  转移给调用者。
- `granit_result_message` 返回进程期有效的静态字符串，调用者不得释放。
- `granit_buffer_map` 返回的地址只在配对 `unmap`、Buffer 销毁或 Renderer 销毁前有效；同一
  Buffer 的映射、flush、写入和销毁必须由调用者排序。映射期间不能调用同步 `write`，但可对
  UPLOAD Buffer 的已映射子范围显式 flush。

## 错误与输出

- 所有可能失败的操作返回 `granit_result`；零表示成功，负值表示失败，异常不会跨越 C ABI。
- 参数为空、范围错误、未知标志位或状态顺序错误通常返回
  `GRANIT_ERROR_INVALID_ARGUMENT`；无效、类型错误或归属错误的句柄返回
  `GRANIT_ERROR_INVALID_HANDLE`。
- 后端或设备不支持合法请求时返回 `GRANIT_ERROR_UNSUPPORTED`；Device Lost 是 Renderer 级
  粘滞错误，首次发生后相关操作持续返回 `GRANIT_ERROR_DEVICE_LOST`。
- 除专门定义为“容量输入/实际大小输出”的查询参数外，调用者只能在返回成功后读取输出值。
- `granit_result_message` 仅用于诊断；程序逻辑应比较结果码，不应解析消息文本。

## 描述结构扩展

- 可扩展描述以 `struct_size` 开头。调用者应使用 `*_DESC_INIT` 或填写其理解的
  `*_VERSION_N_SIZE`，并把保留字段置零。
- 库只读取 `struct_size` 覆盖的已知字段；缺失的已追加字段使用该版本定义的默认语义，超出库已知
  大小的尾部数据被忽略。
- 新版本只能在结构末尾追加字段，并保留旧 `*_VERSION_N_SIZE`。已有字段不得重排、改型或重新
  解释；新枚举和标志位不得复用已有数值。
- 小于最低受支持版本、在字段中间截断或包含不受支持值的描述返回
  `GRANIT_ERROR_INVALID_ARGUMENT` 或 `GRANIT_ERROR_UNSUPPORTED`。

## 线程与回调

对象级并发规则见[线程安全约定](thread-safety.md)。诊断回调在产生消息的线程同步执行，可能从
多个线程并发进入；消息只在回调期间有效，`user_data` 由调用者持有并必须覆盖 Renderer 全生命
周期。回调不得重入产生消息的同一 Renderer。

## Component 边界

本契约只覆盖核心 `granit` C ABI。RenderPipeline、Window、Input 与 Integration 使用独立库和
CMake component；核心稳定不自动冻结这些可选 component。当前稳定等级见
[版本与兼容策略](compatibility.md)。
