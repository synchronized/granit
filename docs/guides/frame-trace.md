<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Frame Trace 诊断

Frame Trace 是开发期内部 JSON Lines 诊断输出，默认关闭，不属于公共 ABI。启用后每行是一个 JSON 对象，
至少包含 `schema_version`、`sequence`、`timestamp_ns`、`kind`，其余字段按事件类型展开。

## 使用

```text
GRANIT_FRAME_TRACE=frame-trace.jsonl
GRANIT_FRAME_TRACE_MAX_EVENTS=4096
```

桌面 Model Viewer 也支持：

```text
granit_sample_model_viewer.exe --asset FlightHelmet.gltf \
  --frame-trace frame-trace.jsonl --renderdoc=frame:3
```

`--renderdoc=trigger` 在 Renderer 初始化后请求一次捕获，`frame:N` 在第 N 次 Present 后请求捕获；
RenderDoc 缺失时仍继续运行并记录诊断事件。RenderDoc 路径可通过 `--renderdoc-path` 指定。

## RenderDoc 手动验收

Windows Vulkan 硬件验收时，先启动 RenderDoc，再运行 Model Viewer：

1. 使用 `--renderdoc=frame:3` 捕获第三次 Present；
2. 在 RenderDoc 中确认捕获包含 Frame、Pass、Pipeline、Resource 和 Draw/Dispatch 标记；
3. 对照同一次运行生成的 Frame Trace，检查 `sequence` 连续、`kind` 合法，且没有后端
   `error`/`validation` 诊断；
4. 关闭 RenderDoc 或指定不存在的 `--renderdoc-path`，确认程序仍能正常启动和渲染，并在
   Frame Trace 中记录不可用诊断，而不是让 Renderer 初始化失败。

RenderDoc 不属于 Granit 的构建、安装或 Runtime Bundle 依赖。Linux 无 RenderDoc 环境只验收
动态探测和 no-op 路径；Emscripten/WebGPU 不提供 RenderDoc 捕获，使用浏览器门禁和 Frame Trace
进行等价诊断。

当前事件包括 Frame Context、Submit/Present、资源创建销毁、Pipeline/Bind Group、Draw/Dispatch、
Timestamp 和诊断信息。Timestamp 或其他后端能力不可用时记录 `availability=unavailable`，不把缺失能力
伪装成零值。

Trace 是 0.x 开发格式，不承诺跨版本持久化兼容。仓库测试使用
`granit_frame_trace_validator` 做无第三方依赖的逐行 JSONL 解析和 schema 校验；Windows 测试另外使用
系统 JSON 解析器交叉验证。校验器不依赖 RenderDoc、Vulkan 或 Dawn。
