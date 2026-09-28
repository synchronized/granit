<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-28 S-74 GPU Marching Cubes 验收

## 结论

S-74A～F 已完成本地验收。Vulkan 与浏览器 WebGPU 均能用公共 Indirect 命令和帧级 Transient
Buffer 执行 Density → Polygonize → Finalize → Draw 的完整动态几何链；06 教程在两个后端生成
非零网格并通过多帧和 Resize 验证。

## 交付内容

- C/C++ Command Recorder 增加单次 Draw、Indexed Draw 与 Dispatch Indirect；
- Frame Context 增加按真实 Frame Slot 复用的 Transient Buffer Slice；
- Vulkan 建立 Transfer、Compute、Vertex 和 Indirect 状态依赖，WebGPU 映射对应 Usage 与 Pass 顺序；
- `06_marching_cubes` 使用三个 Compute Pass、Atomic 容量保护、Draw Indirect、异步统计回读、统一
  Tutorial Runtime 和 Orbit Camera；
- VTK Marching Cubes 查表固定来源提交并保留 BSD-3-Clause 许可说明；
- Emscripten CI 增加 06 教程产物审计与浏览器行为任务。

浏览器第一次验收发现只读 Storage Buffer 的 Shader 声明与公共 Layout 中可写 Storage 类型不一致，
以及同步 Buffer Map 会阻塞浏览器事件循环。最终实现统一使用可移植 Storage Binding，并把诊断统计
改为 Async Readback；绘制始终由 GPU Indirect 参数驱动。

## 本地验证

| 环境 | 验证 | 结果 |
|---|---|---|
| Windows Clang shared Debug | 完整构建；`ctest --preset windows-clang-debug` | 102/102 通过 |
| Windows Clang static Debug | 完整构建；`ctest --preset windows-clang-static-debug` | 92/92 通过 |
| Emscripten Debug | 完整构建；`ctest --preset emscripten-debug` | 16/16 通过 |
| Chrome WebGPU | Tutorial 01～06 浏览器脚本 | 全部通过 |
| 文档与格式 | `granit.documentation.links`；`git diff --check` | 通过 |

06 浏览器测试同时验证 Runtime Ready、至少三帧、非零生成顶点、非空 Canvas、Resize 后继续出帧和
WebGPU Validation 无错误。Linux、MSVC、安装 SDK 与发布产物继续由同一 Pull Request 的远端门禁
和发布工作流验证。

## 已知限制

- 每个 Indirect API 只执行一条命令，不提供 Multi Draw 或 Count Buffer；
- Transient Arena 首版按 Frame Slot 和精确 Usage 分页，不进行跨资源内存别名；
- 浏览器当前不满足 Granit Timestamp Query 契约，教程显示不可用；
- Portable Profile 不提供 Line Polygon Mode，Wireframe 控件明确禁用。
