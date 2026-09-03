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

## 设备限制查询

动态 Uniform Buffer 的偏移对齐由具体设备决定，调用方不得固定假定为 256。创建 Renderer 后，
使用同一个可扩展结构查询限制快照：

```c
granit_renderer_limits limits = GRANIT_RENDERER_LIMITS_INIT;
granit_result result = granit_renderer_get_limits(renderer, &limits);
```

`uniform_buffer_offset_alignment` 是基础 Offset 和动态 Offset 必须满足的对齐值；
`max_uniform_buffer_binding_size` 是单个 Uniform Buffer Binding Range 的最大字节数。Uniform Arena
步长可按 `(size + alignment - 1) / alignment * alignment` 向上对齐，计算时需要防止整数溢出。

`framebuffer_sample_counts` 是通用颜色与深度附件共同支持的样本数位集合，可用
`GRANIT_SAMPLE_COUNT_1/2/4/8` 按位检查；具体格式仍可能施加更严格限制，资源或 Pipeline 创建失败
时不会静默降低样本数。`max_sampler_anisotropy` 至少为 1；值为 1 表示不能启用各向异性过滤。
C++ `renderer_limits::supports_sample_count` 提供对应的便捷检查。

调用者必须设置 `struct_size`，当前至少为 `GRANIT_RENDERER_LIMITS_VERSION_1_SIZE`。查询接受更大的
未来结构并只写当前版本已知字段；结构过小或空指针返回 `GRANIT_ERROR_INVALID_ARGUMENT`，失效
Renderer 返回 `GRANIT_ERROR_INVALID_HANDLE`。限制来自 Renderer 创建时保存的不可变能力快照，
查询不会再次访问驱动。

## 后端选择与信息

`granit_renderer_desc::backend` 可设置 `AUTO`、`VULKAN` 或 `WEBGPU`。显式选择不会回退到
另一后端；桌面 WebGPU 可通过 `backend_library_path` 指定 Granit WebGPU Provider 的绝对路径。
路径只在创建调用期间借用，拒绝相对路径。路径为空时只检查 Granit Core 模块旁及安装布局中固定
的 `granit/backends` 位置，不扫描当前工作目录或系统 `PATH`。静态链接应用若没有把 Provider
放在这些固定相对位置，应显式传入打包后的绝对路径。旧版描述不含这些尾部字段时保持 `AUTO`。

桌面 `AUTO` 先尝试 Vulkan；仅当结果为后端不可用、驱动不兼容、没有合适设备或请求能力不支持时
尝试 WebGPU。内存不足、参数错误和内部错误不会触发回退。两次尝试均失败时返回 Vulkan 的结果，
并通过 Diagnostic Callback 记录各候选失败原因。显式选择只尝试指定后端。

创建成功后使用 `granit_renderer_get_info` 查询实际后端。Adapter 名称采用两次查询：第一次将
`adapter_name` 和容量设为零以取得所需字节数，第二次提供包含结尾零字符的缓冲区。名称长度不含
结尾零字符；名称、Vendor ID 或 Device ID 不可用时返回空值。这些元数据只用于诊断和性能记录，
不应作为渲染行为分支条件。C++ 包装通过 `renderer::get_info(renderer_info&)` 完成缓冲区管理。

Emscripten 使用静态 WebGPU Provider，`AUTO` 与 `WEBGPU` 都选择该后端；显式 Vulkan 返回
`GRANIT_ERROR_BACKEND_UNAVAILABLE`，任何动态库路径均返回 `GRANIT_ERROR_INVALID_ARGUMENT`。

## 资源统计

关闭 Renderer 前，可查询仍由调用方持有的公开子资源：

```c
granit_renderer_resource_stats stats = GRANIT_RENDERER_RESOURCE_STATS_INIT;
granit_result result = granit_renderer_get_resource_stats(renderer, &stats);
if (result == GRANIT_SUCCESS && stats.total_live_count != 0) {
  /* 先销毁仍存活的子资源。 */
}
```

`total_live_count` 汇总 Buffer、Texture、Texture View、Sampler、Shader、布局、Bind Group、
Pipeline、Surface、Swapchain、Command Recorder、Frame Context、活动 Frame、Timestamp Query Pool
和 Upload Batch。各字段可用于定位具体类型。Swapchain Backbuffer 的 Texture/View 是借用句柄，
不计入公开 Texture/View 数量；因此调用方可用 `total_live_count == 0` 作为关闭前检查，且无需了解
未来追加的分类字段。

公开句柄销毁后会立即从存活数量中移除。Vulkan 后端可能继续持有其原生资源，直到相关 GPU 提交
完成；这部分由 `pending_retirement_count` 单独报告，不代表调用方泄漏，也不要求关闭前为零。
查询只读取 Registry 和后端队列状态，不等待 GPU、不推进事件，也不执行用户回调。空指针或过小
结构返回 `GRANIT_ERROR_INVALID_ARGUMENT`，无效 Renderer 返回 `GRANIT_ERROR_INVALID_HANDLE`。

## 生命周期状态

Renderer 提供非阻塞状态查询和事件推进接口：

```c
granit_renderer_status status = GRANIT_RENDERER_STATUS_INIT;
granit_result result = granit_renderer_get_status(renderer, &status);
if (result == GRANIT_SUCCESS && status.state == GRANIT_RENDERER_STATE_INITIALIZING) {
  result = granit_renderer_process_events(renderer);
}
```

状态包括 `INITIALIZING`、`READY`、`FAILED` 和 `DEVICE_LOST`。`failure_result` 仅在失败或设备
丢失状态下保存稳定结果码；详细原因仍由诊断回调提供。状态查询不会等待或执行用户回调，
`granit_renderer_process_events` 也只非阻塞地推进后端已完成事件。当前 Vulkan Renderer 创建成功后
立即为 `READY`；该模型同时为异步 WebGPU 初始化保留统一入口。

需要创建窗口 Surface 时，通过 `surface_types` 提前声明窗口系统。当前公共入口支持 Win32、XCB、
Wayland 和 Canvas；实际可用集合取决于所选后端、平台及 Provider 能力。具体创建方式见
[surface.md](surface.md)。

## C++ API

```cpp
granit::renderer renderer;
const auto result = renderer.initialize({.application_name = "example"});
if (result.failed()) {
  // 处理错误
}
```

`granit::renderer` 不使用异常，是 move-only RAII 类型。成功初始化后析构函数自动销毁；`reset`
可提前释放。`native_handle` 只返回 Granit C 句柄，用于 C/C++ 层互操作，并非 Vulkan 句柄。

C++ 调用方通过 `renderer::get_limits(renderer_limits&)` 查询相同限制快照，并通过
`renderer::get_resource_stats(renderer_resource_stats&)` 查询资源统计，通过
`renderer::get_status(renderer_status&)` 和 `renderer::process_events()` 使用相同的非阻塞生命周期
模型；结果和错误语义与 C API 一致。

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

当调用方提供的 Renderer 有效时，代表性的公共 API 参数错误、资源类型错误、失效句柄和跨
Renderer 句柄会通过 `validation` 类别补充 API 名、参数名、期望资源类型或归属约束。结果码仍是
程序判断失败类型的唯一稳定依据，诊断文本只用于日志和定位，不应被解析。Renderer 句柄本身无效
时无法找到与其绑定的回调，因此只返回 `GRANIT_ERROR_INVALID_HANDLE`。

启用验证后，可通过 `granit_renderer_set_object_name` 为公开 GPU 资源句柄设置 UTF-8 调试名称，
C++ 包装对应 `renderer::set_object_name`。Registry 会在内部识别对象类型并提交给 Vulkan Debug
Utils，公共接口不暴露 Vulkan 类型或原生句柄。名称只在调用期间借用，长度范围为 1 到 4096
字节且不能包含内嵌零字符；失效或跨 Renderer 句柄返回 `GRANIT_ERROR_INVALID_HANDLE`。
未启用验证或对象是 Frame、Upload Batch 等纯管理对象时返回 `GRANIT_ERROR_UNSUPPORTED`。

Renderer 首次观察到 `VK_ERROR_DEVICE_LOST` 时，会通过 `device` 类别发出一条 error 诊断，内容
包含触发操作、稳定的 Granit 结果、Vulkan 后端结果、验证状态和 Renderer domain。Device Lost
保持为 Renderer 级粘滞状态；后续操作继续返回 `GRANIT_ERROR_DEVICE_LOST`，但不会重复输出报告。

## 生命周期与线程安全

公开 renderer 由进程内 registry 管理。句柄销毁后立即对新操作失效，重复销毁返回
`GRANIT_ERROR_INVALID_HANDLE`。内部操作通过共享所有权保持 renderer 存活，因此与销毁已经并发
开始的操作可以安全结束。实际 Vulkan device 析构和等待发生在 registry 锁之外。
