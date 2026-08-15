<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Renderer

## 定位

Renderer 是 Granit 当前公开的根对象。创建成功意味着 Vulkan loader、instance、物理设备、
逻辑设备和 graphics queue 均已就绪。公共接口只暴露 Granit 整数句柄，不暴露 Vulkan 对象。

## C API

使用初始化宏建立带正确尺寸和版本的描述：

```c
granit_renderer_desc desc = GRANIT_RENDERER_DESC_INIT;
const char application_name[] = "example";
desc.application_name = application_name;
desc.application_name_length = sizeof(application_name) - 1;

granit_renderer renderer = GRANIT_NULL_HANDLE;
granit_result result = granit_renderer_create(&desc, &renderer);
if (result == GRANIT_SUCCESS) {
  granit_renderer_destroy(renderer);
}
```

`application_name` 使用指针和显式长度，调用期间借用；Granit 会在 Vulkan instance 创建前复制。
空指针和零长度使用默认名称。非空指针必须提供非零长度，且指定范围内不能含嵌入的零字符。

`struct_size` 支持描述结构向后兼容。当前至少要求 `GRANIT_RENDERER_DESC_VERSION_1_SIZE`，未来
新增字段只能追加到结构末尾；旧库会忽略超出已知范围的尾部字段。

需要创建窗口 Surface 时，通过 `surface_types` 提前声明窗口系统。当前支持
`GRANIT_SURFACE_TYPE_WIN32_BIT`；具体创建方式见 [surface.md](surface.md)。

## C++ API

```cpp
granit::renderer renderer;
const auto result = renderer.initialize({.application_name = "example"});
if (granit::failed(result)) {
  // 处理错误
}
```

`granit::renderer` 不使用异常，是 move-only RAII 类型。成功初始化后析构函数自动销毁；`reset`
可提前释放。`native_handle` 只返回 Granit C 句柄，用于 C/C++ 层互操作，并非 Vulkan 句柄。

## Validation

C API 通过 `GRANIT_RENDERER_ENABLE_VALIDATION_BIT` 请求验证层；C++ 使用
`renderer_desc::enable_validation`。请求验证但运行环境缺少 Khronos validation layer 或 debug
utils extension 时，创建返回 `GRANIT_ERROR_UNSUPPORTED`，不会静默关闭验证。

Vulkan Validation Layer 负责 Vulkan API、同步和对象规则，但无法判断 Granit 用户是否遗漏显式
销毁公开资源。Granit 提供独立生命周期验证：验证模式下 Renderer 销毁会汇总尚存的用户拥有
资源，然后仍完成句柄失效和级联清理。Swapchain Backbuffer 等借用资源不作为用户泄漏报告。
具体方案见 [V-01 生命周期验证计划](../plans/V-01-lifetime-validation.md)。

验证模式下，销毁仍拥有用户子资源的父对象也会输出诊断：Texture 会报告用户创建的 View，
Surface 会报告仍存活的 Swapchain。诊断不会改变销毁结果，子资源仍按依赖顺序失效和释放。

## 诊断回调

`granit_renderer_desc` V4 可以设置 `diagnostic_callback` 和 `diagnostic_user_data`。回调接收稳定的
严重级别、消息类别以及不保证零结尾的 UTF-8 文本；文本只在回调期间有效。C++ 的
`granit::renderer_desc` 对应字段为 `diagnostics` 和 `diagnostic_user_data`。

回调在产生诊断的线程同步执行，Vulkan Validation 消息可能从多个线程并发进入，因此接收器必须
线程安全，并且只应进行有界复制或投递到调用方自己的日志队列。回调不得重入产生消息的同一
Renderer，`user_data` 的有效期必须覆盖 Renderer 创建、使用与销毁全过程。

未设置回调时，Granit 将消息写入标准错误流。回调为空而 `diagnostic_user_data` 非空属于无效描述。
当前类别包括 general、validation、performance、lifecycle 和 device；它们不复用 Vulkan 数值。

## 生命周期与线程安全

公开 renderer 由进程内 registry 管理。句柄销毁后立即对新操作失效，重复销毁返回
`GRANIT_ERROR_INVALID_HANDLE`。内部操作通过共享所有权保持 renderer 存活，因此与销毁已经并发
开始的操作可以安全结束。实际 Vulkan device 析构和等待发生在 registry 锁之外。
