<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 使用异步回读与 Pipeline 预热

## 适用场景

异步 Readback Batch 用于截图、拾取和离屏结果导出；Pipeline Warmup Batch 用于在加载阶段提前
触发已知图形与计算 Pipeline 的创建。两者都返回 `granit_async_operation`，调用方通过事件循环
轮询，不需要阻塞等待 GPU。

## 异步回读

1. 创建有界 `granit::readback_batch`，选择紧密或后端原始 Texture 布局。
2. 记录一个或多个 Buffer/Texture 区域，然后调用 `submit_async()`。
3. 每帧调用 `renderer.process_events()` 并查询操作状态。
4. 成功后先查询单项元数据，再把结果复制到调用方拥有的内存。

紧密布局适合图片编码和像素比较；后端布局保留 `bytes_per_row` 与 `rows_per_image`，适合需要原始
对齐信息的工具。提交会保留源资源，销毁公开异步句柄不会提前释放 GPU 正在使用的资源。

## Pipeline 预热

1. 创建 `granit::pipeline_warmup_batch` 并设置最大条目数。
2. 添加现有图形或计算 Pipeline 描述。提交会复制描述并保留 Shader 与 Pipeline Layout；提交成功
   后调用方可以销毁原句柄。
3. 提交后在加载循环中轮询；每次推进至多处理一个尚未命中的条目。
4. 查询每项结果、缓存命中标记和 32 字节稳定键。某项失败不会中止同批其他条目。

稳定键由规范化 Pipeline 状态、Shader 内容 ID、入口、后端和设备能力构成，不包含进程内资源
句柄。Vulkan 会复用原生 Pipeline Cache，并在私有后台任务中执行冷创建；浏览器 WebGPU 使用
原生异步 Render/Compute Pipeline 回调。两者都声明 `NON_BLOCKING_PIPELINE_WARMUP`，提交和单次
事件推进不会执行同步冷编译。

## 能力判断

通过 `renderer.get_limits()` 得到 `renderer_limits`：

- `supports_async_readback()`：支持异步批量回读。
- `supports_pipeline_warmup()`：支持批量 Pipeline 预热。
- `supports_non_blocking_pipeline_warmup()`：单次事件推进保证不执行同步 Pipeline 编译。

不应通过 Vulkan/WebGPU 后端名称推断上述能力。

异步 Upload/Readback Batch 返回 `NOT_READY` 表示后端暂时没有空闲槽位。此结果不会清空批次或
创建残留操作；调用方应继续处理事件，在后续帧重试同一批次，不能把它当作永久失败。

## 取消与销毁

尚未开始的操作可以立即取消；正在运行的预热会保留已完成结果并取消剩余条目。异步句柄销毁后，
内部任务仍会由 Renderer 安全排空。关闭 Renderer 前应先销毁批次和公开操作句柄，以便资源统计
归零并避免生命周期诊断。
