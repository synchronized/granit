<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Swapchain

## 定位

Swapchain 管理 Surface 对应的窗口后缓冲图像。公开 API 不暴露 Vulkan 图像、格式或交换链句柄；
当前阶段提供创建、查询、重建和销毁，为后续帧获取与呈现接口建立稳定生命周期。

## 创建

```c
granit_swapchain_desc desc = GRANIT_SWAPCHAIN_DESC_INIT;
desc.width = window_width;
desc.height = window_height;
desc.present_mode = GRANIT_PRESENT_MODE_FIFO;

granit_swapchain swapchain = GRANIT_NULL_HANDLE;
granit_result result = granit_swapchain_create(renderer, surface, &desc, &swapchain);
```

宽度和高度不能为零。`minimum_image_count` 为零时由 Granit 请求 Surface 最小图像数加一，并受
驱动能力限制；非零值允许 1 至 16，是期望的最小数量。实际数量通过
`granit_swapchain_get_info` 查询。

支持以下平台无关呈现模式：

- `GRANIT_PRESENT_MODE_FIFO`：垂直同步，Vulkan 保证支持，也是默认值。
- `GRANIT_PRESENT_MODE_MAILBOX`：低延迟三缓冲倾向；不可用时回退 FIFO。
- `GRANIT_PRESENT_MODE_IMMEDIATE`：允许撕裂；不可用时回退 FIFO。

实际采用的模式会写入 `granit_swapchain_info.present_mode`。颜色格式暂由 Granit 选择，优先使用
BGRA8 sRGB；在渲染目标抽象完成前不进入公共 API。

## 重建与查询

窗口尺寸变化后调用 `granit_swapchain_recreate`。新 Swapchain 完整创建成功后才替换旧对象；失败
时旧对象仍然有效。驱动决定固定窗口范围时，实际尺寸可能与请求不同，应查询：

```c
granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
granit_swapchain_get_info(renderer, swapchain, &info);
```

最小化窗口导致有效范围为零时，重建返回 `GRANIT_ERROR_INVALID_ARGUMENT`，调用者应等待窗口恢复。
Surface 丢失返回 `GRANIT_ERROR_SURFACE_LOST`；交换链过期返回 `GRANIT_ERROR_OUT_OF_DATE`。

## 生命周期和线程安全

Swapchain 属于创建它的 Renderer 和 Surface，整数句柄会同时验证资源类型和 Renderer domain。
销毁 Surface 会先销毁其全部 Swapchain；销毁 Renderer 会按 Swapchain、Surface、Device、Instance
的顺序清理。推荐仍按相反创建顺序显式销毁。

不同 Renderer 的资源操作可以并行。同一 Renderer 内的 Surface 和 Swapchain Vulkan 操作目前由
局部资源锁串行化，Registry 锁只保护句柄和所有权映射，不在持锁期间创建或销毁 Vulkan 对象。
不要让父对象销毁与其子对象操作并发执行。

内部 Vulkan acquire/present 原语已经完成。公共入口将在 F-06B 通过短生命周期 Frame 令牌与帧槽
Semaphore、Recorder 提交和 backbuffer Layout 转换关联，避免隐式绑定“最近一次 acquire”。

## Backbuffer 资源

`granit_swapchain_get_backbuffer` 可按索引取得 Swapchain 拥有的 Texture 和默认 View。返回句柄是
借用资源，不能通过 Texture/View 销毁函数单独销毁。Swapchain 重建或失效后，所有旧句柄立即
失效；调用者应重新查询。当前接口只提供资源身份，实际当前图像由后续 acquire 接口确定。
