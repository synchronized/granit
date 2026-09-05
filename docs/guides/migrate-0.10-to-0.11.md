<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.10 迁移到 0.11

0.11.0 补齐浏览器 WebGPU 的可移植传输和 Mipmap，并发布正式浏览器 Model Viewer。Core 的
Renderer Limits 追加 Timestamp Query 能力位；RenderPipeline、Shader 和持久化资产格式不变。

## 更新设备能力判断

重新编译 Consumer，并通过 `granit_renderer_get_limits` 取得完整结构。使用 C API 时检查
`GRANIT_RENDERER_FEATURE_TIMESTAMP_QUERY_BIT`；C++ 可调用
`renderer_limits::supports_timestamp_queries()`。浏览器 WebGPU 当前明确返回不支持，不应显示零值
GPU 时间，也不应按后端名称猜测。

## 运行浏览器模型查看器

Emscripten 构建会生成 `granit_model_viewer_web.html`。产物必须通过 HTTP 服务访问；默认模型可用
`?model=<URL>` 覆盖，远程模型及外部资源须允许跨域访问。详细步骤见
[浏览器 WebGPU 示例](webgpu-browser-example.md)。

## CMake 与 ABI

将 `find_package(granit 0.10 ...)` 更新为 `find_package(granit 0.11 ...)`。本版本向
`granit_renderer_limits` 末尾追加 `supported_features`；Granit 仍处于 0.x，Consumer 必须重新编译。
