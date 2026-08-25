<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Swapchain

## 定位

Swapchain 管理 Surface 对应的窗口后缓冲图像。公开 API 不暴露 Vulkan 图像或交换链句柄；
当前阶段提供创建、查询、重建、帧获取、呈现和销毁，并隐藏 WSI 同步及图像布局细节。
呈现完成信号量按 Swapchain 图像管理，图像重新获取前不会复用，避免与呈现引擎仍在等待的
二进制信号量发生冲突。

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

实际采用的模式会写入 `granit_swapchain_info.present_mode`。Granit 选择的后端无关颜色格式写入
`granit_swapchain_info.format`；创建对应 Graphics Pipeline 时应使用该格式。

## 重建与查询

窗口尺寸变化后调用 `granit_swapchain_recreate`。新 Swapchain 完整创建成功后才替换旧对象；失败
时旧对象仍然有效。驱动决定固定窗口范围时，实际尺寸可能与请求不同，应查询：

```c
granit_swapchain_info info = GRANIT_SWAPCHAIN_INFO_INIT;
granit_swapchain_get_info(renderer, swapchain, &info);
```

最小化窗口导致有效范围为零时，重建返回 `GRANIT_ERROR_NOT_READY`，调用者应暂停该窗口的帧循环，
等待窗口恢复到非零尺寸后再重建，不应忙循环。此时旧 Swapchain 和 backbuffer 句柄保持有效，但在
成功重建前不应继续 acquire。创建全新 Swapchain 时传入零尺寸仍返回
`GRANIT_ERROR_INVALID_ARGUMENT`。
Surface 丢失返回 `GRANIT_ERROR_SURFACE_LOST`；交换链过期返回 `GRANIT_ERROR_OUT_OF_DATE`。
SURFACE_LOST 是粘滞终止状态，不能通过对原 Swapchain 调用 recreate 恢复；应停止该窗口的帧
循环，销毁旧 Swapchain 和 Surface，在平台窗口句柄仍有效时重新创建两者。DEVICE_LOST 表示整个
Renderer 的设备已不可继续使用，应停止所有窗口帧路径并重建 Renderer 及其资源。

## 生命周期和线程安全

Swapchain 属于创建它的 Renderer 和 Surface，整数句柄会同时验证资源类型和 Renderer domain。
销毁 Surface 会先销毁其全部 Swapchain；销毁 Renderer 会按 Swapchain、Surface、Device、Instance
的顺序清理。推荐仍按相反创建顺序显式销毁。

不同 Renderer 的资源操作可以并行。同一 Renderer 内的 Surface 和 Swapchain Vulkan 操作目前由
局部资源锁串行化，Registry 锁只保护句柄和所有权映射，不在持锁期间创建或销毁 Vulkan 对象。
不要让父对象销毁与其子对象操作并发执行。

重建、显式销毁、Surface 级联销毁和 Renderer 关闭会先等待 graphics/present Queue 空闲，再
释放旧 backbuffer View 与 Swapchain。该阻塞只发生在低频 WSI 生命周期路径；普通 Buffer、
Texture 和 Sampler 销毁继续使用提交序号退役，不会调用 Queue/Device Wait Idle。

公共帧循环使用短生命周期 Frame 令牌与帧槽 Semaphore、Recorder 提交和 backbuffer Layout
转换关联，避免隐式绑定“最近一次 acquire”：

```cpp
granit::acquired_frame frame;
swapchain.acquire(frame);
swapchain.backbuffer(frame.image_index, texture, view);
recorder.begin();
// 使用 view 录制 Dynamic Rendering。
recorder.end();
recorder.submit(frame);
swapchain.present(frame);
```

成功 acquire 后，可在 Frame 存活期间查询它使用的真实在途帧槽：

```c
granit_frame_info frame_info = GRANIT_FRAME_INFO_INIT;
granit_result result = granit_frame_get_info(renderer, swapchain, frame, &frame_info);
```

`frame_slot` 的范围是 `[0, frame_slot_count)`，来自 Renderer 内部实际获取的帧槽，可用于选择同寿命
的 Recorder 或临时上传资源。它不是 `image_index`，也不能由帧序号或句柄取模推导。查询只借用
Frame；present、cancel、Swapchain 失效或 Renderer 销毁后，旧 Frame 查询返回
`GRANIT_ERROR_INVALID_HANDLE`。Renderer、Swapchain 与 Frame 不属于同一对象关系时也返回该错误。
调用者必须使用 `GRANIT_FRAME_INFO_INIT`，预留字段由实现写为零。

`needs_recreate` 在 acquire 或 present 遇到 SUBOPTIMAL 时为 true；OUT_OF_DATE 通过结果码返回。
成功 acquire 后必须完成 submit 和 present，或者调用 `granit_frame_cancel` / `swapchain.cancel`。
取消操作会提交最小布局转换并归还已获取图像，因此不是简单丢弃令牌。C++ `acquired_frame` 离开
作用域时会自动呈现已提交帧，或取消尚未提交的帧；析构结果无法返回，需处理错误或立即重建时
仍应显式调用。活动 Frame 存在时不能重建或销毁对应 Swapchain。

## Backbuffer 资源

`granit_swapchain_get_backbuffer` 可按索引取得 Swapchain 拥有的 Texture 和默认 View。返回句柄是
借用资源，不能通过 Texture/View 销毁函数单独销毁。Swapchain 重建或失效后，所有旧句柄立即
失效；调用者应重新查询。当前接口只提供资源身份，实际当前图像由后续 acquire 接口确定。
