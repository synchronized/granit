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
2. 添加现有图形或计算 Pipeline 描述。描述中的数组会被复制，但 Shader 与 Pipeline Layout 句柄
   必须保持到异步操作结束。
3. 提交后在加载循环中轮询；每次推进至多处理一个尚未命中的条目。
4. 查询每项结果、缓存命中标记和 32 字节稳定键。某项失败不会中止同批其他条目。

稳定键由规范化 Pipeline 状态、Shader 内容 ID、入口、后端和设备能力构成，不包含进程内资源
句柄。Vulkan 会复用原生 Pipeline Cache；WebGPU 当前使用分步预热，但不声明
`NON_BLOCKING_PIPELINE_WARMUP`，调用方应在加载阶段调用而不是在交互帧中首次启动。

## 能力判断

通过 `renderer.get_limits()` 得到 `renderer_limits`：

- `supports_async_readback()`：支持异步批量回读。
- `supports_pipeline_warmup()`：支持批量 Pipeline 预热。
- `supports_non_blocking_pipeline_warmup()`：单次事件推进保证不执行同步 Pipeline 编译。

不应通过 Vulkan/WebGPU 后端名称推断上述能力。

## 取消与销毁

尚未开始的操作可以立即取消；正在运行的预热会保留已完成结果并取消剩余条目。异步句柄销毁后，
内部任务仍会由 Renderer 安全排空。关闭 Renderer 前应先销毁批次和公开操作句柄，以便资源统计
归零并避免生命周期诊断。
