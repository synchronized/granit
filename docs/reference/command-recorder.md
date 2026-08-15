<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Command Recorder

Command Recorder 是 Granit 的显式命令录制上下文。它使用整数句柄隔离 Vulkan Command Pool 和
Command Buffer，允许不同 Recorder 在不同线程并行录制。

## C API

```c
granit_command_recorder_desc desc = GRANIT_COMMAND_RECORDER_DESC_INIT;
granit_command_recorder recorder = GRANIT_NULL_HANDLE;

granit_command_recorder_create(renderer, &desc, &recorder);
granit_command_recorder_begin(renderer, recorder);
granit_command_recorder_copy_buffer(
    renderer, recorder, source, destination, regions, region_count);
granit_command_recorder_fill_buffer(renderer, recorder, destination, 0, 256, 0);
granit_command_recorder_end(renderer, recorder);
granit_command_recorder_submit(renderer, recorder);
granit_command_recorder_reset(renderer, recorder);
granit_command_recorder_destroy(renderer, recorder);
```

`granit_command_recorder_copy_texture` 在两个 Texture 间复制显式区域。首版要求源和目标格式
相同、均为单采样非深度颜色纹理，并分别声明 `TRANSFER_SOURCE` 与 `TRANSFER_DESTINATION`
用途；同一纹理内复制暂不支持。

`granit_command_recorder_copy_buffer_to_texture` 从带 `TRANSFER_SOURCE` 用途的 Buffer 复制到
带 `TRANSFER_DESTINATION` 用途的 Texture。`layout.offset` 是源 Buffer 偏移，行跨度与区域约束
和 Texture 同步写入一致。该接口适合调用方管理上传内存和批量 Recorder 的高级路径；普通上传
仍优先使用 `texture.write` 或 Upload Batch。

`submit` 异步提交 executable Recorder。成功后 Recorder 进入 pending；对它调用 `reset` 会等待
GPU 完成，再重置 Command Pool。Renderer 的 `frames_in_flight` 决定最多保留多少个在途帧槽，
默认值为 2，有效范围为 1 到 4。

多个独立 Recorder 可以原子校验后批量提交：

```c
granit_command_recorder recorders[] = {first, second, third};
granit_command_recorder_submit_batch(renderer, recorders, 3);
```

数组不能为空，Recorder 必须互不重复且属于同一 Renderer。成功时按数组顺序执行，整批共享
一次 Queue 提交、一个 Fence 和一个 submission serial；失败时不会提交其中任何 Recorder。

Buffer Copy 支持一次传入多个区域。参与命令的 Buffer 会由 Recorder 保持内部强引用，因此录制
后销毁公开 Buffer 句柄不会造成悬空 Vulkan 对象；reset 或 destroy 会释放这些引用。

Graphics Pipeline 可以单独绑定；Bind Group 通过 Pipeline Layout 和起始组序号批量绑定。Recorder
会保持 Pipeline、Pipeline Layout 与 Bind Group，直至提交完成并重置。

Viewport 与 Scissor 支持批量设置。Vertex/Index Buffer 在 Dynamic Rendering 开始前绑定，以便
自动屏障在渲染区域外完成；Draw 和 Draw Indexed 只能在渲染区域内录制。

## C++20

```cpp
granit::command_recorder recorder;
if (recorder.initialize(renderer.native_handle()) == granit::result::success) {
  recorder.begin();
  recorder.end();
  recorder.submit();
  recorder.reset();
}

std::array<granit::command_recorder, 3> recorders;
granit::command_recorder::submit_batch(recorders);
```

包装类型无异常、不可复制且可以移动。`reset()` 重置录制状态，`destroy()` 销毁 Recorder 句柄。

## 状态与线程

- create 后处于 initial。
- begin 进入 recording。
- end 进入 executable。
- submit 进入 pending，GPU 完成后回到 executable。
- submit_batch 成功时整批进入 pending；任一 Recorder 的 reset 都会等待共享 Fence 并完成整批。
- reset 回到 initial。
- 单个 Recorder 不能并发调用，但可以在无并发时移交线程。
- 不同 Recorder 可以并行录制。
- 不同线程可以并行创建独立 Buffer 并上传初始数据；Queue 提交由 Renderer 内部串行化，提交顺序
  决定跨 Recorder 的资源状态顺序。
- 状态错误返回 `GRANIT_ERROR_INVALID_ARGUMENT`。

详细约束见 [F-01 计划](../plans/F-01-command-recorder.md)。
