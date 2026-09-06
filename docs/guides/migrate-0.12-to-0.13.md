<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.12 迁移到 0.13

0.13.0 增加后端无关的异步 GPU 操作，并让浏览器 WebGPU 在设备支持时提供 Timestamp Query。
Shader、Material 和 Environment 持久化格式没有变化。

## 必须操作

1. 重新编译应用及使用 Granit C/C++ 头文件的模块。
2. 将 `find_package(granit 0.12 ...)` 更新为 `find_package(granit 0.13 ...)`。
3. 浏览器代码读取 Timestamp 时改用 `granit_timestamp_query_pool_get_results_async`，轮询
   `granit_async_operation_get_status`，成功后再调用 `granit_timestamp_query_pool_copy_results`。
4. 创建 Timestamp Query Pool 前查询 `GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT`；能力缺失是
   正常设备差异，不应按操作系统或后端名称推断。

## 生命周期要求

异步操作属于创建它的 Renderer。调用方应在取得结果后显式销毁操作；C++20 包装会通过 RAII
销毁。取消是请求，不保证撤销已经提交的 GPU 工作。关闭 Renderer 前仍应先销毁调用方持有的
异步操作和其他子资源。

## 无需操作

- Vulkan 代码可以继续使用同步 Timestamp 入口，但跨后端代码应优先使用异步入口。
- 无需重新生成 `.grshader`、`.grmat` 或 `.grenv` 资产。
- Model Viewer 的网络、解析和任务调度仍属于示例层，不需要迁入应用的 Granit 公共 API。
