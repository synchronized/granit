<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-16：WebGPU 收敛到 Emscripten 浏览器

## 状态

- 实现状态：实现中
- 前置依赖：S-10、S-12、S-15
- 优先级：P1

## 背景与目标

按照 [ADR-005](../decisions/ADR-005-browser-only-webgpu.md)，Granit 桌面只保留 Vulkan，WebGPU
只服务 Emscripten 浏览器。本任务删除桌面 Dawn 和动态 Provider 成本，同时保留统一 HAL、
Registry、Shader 资产与浏览器模型查看器。

## 非目标

- 不删除公共 WebGPU 后端枚举或浏览器所需资源、命令和 Pipeline 能力。
- 不改变 Vulkan 行为、公共资源所有权或句柄校验语义。
- 不在本任务引入 Android WebGPU、wgpu-native 或另一套浏览器 Registry。
- 不删除历史验收记录。

## 已确认决策

- Emscripten WebGPU 作为静态内部后端直接实现私有 HAL。
- 删除 Granit 后端插件 ABI、Loader、桌面 Dawn 构建选项和 SDK 工作流。
- 删除 `backend_library_path` 公共配置；项目尚未发布，不保留无用途的兼容字段。
- 当前 Guide 只说明桌面 Vulkan和浏览器 WebGPU；历史 Record 保留原有事实与时间背景。

## 实施顺序

### S-16A：构建与文档范围收敛

**状态：已完成。**

1. 删除 Dawn SDK、桌面集成工作流、构建选项和安装组件。
2. 更新构建、模型查看器、示例和架构文档。

### S-16B：移除动态插件边界

1. 删除插件动态加载、平台共享库适配和插件 ABI 测试。
2. 将 Emscripten WebGPU 状态直接连接到静态后端实现。
3. 移除公共 Renderer 描述中的后端动态库路径。

### S-16C：验收

1. Windows/Linux Vulkan 完整构建、测试和安装 Consumer 通过。
2. Emscripten 浏览器 Fixture、输入和 model-viewer 测试通过。
3. 静态边界检查确认桌面目标不再包含 Dawn、WebGPU Plugin 或 Provider Loader。

## 测试与验收

- 普通桌面配置不声明 Dawn 包、不生成 WebGPU 插件，也不链接 WebGPU 实现。
- 浏览器端通过公共 Renderer API 创建 WebGPU、Canvas Surface 和 Swapchain。
- Vulkan 与浏览器 WebGPU 继续消费同一 Shader 资产和高层渲染数据。
- 文档、C/C++ 公共头测试、ABI 快照及完整平台测试同步更新。

## 风险与未决问题

- 当前 Emscripten 实现复用了插件函数表；直接 HAL 化需要分阶段迁移并保持浏览器测试持续通过。
- 删除 `backend_library_path` 会改变尚未发布的 ABI，应在本任务结束时统一更新快照，不重复升版。
- 浏览器问题不再能用桌面 Dawn 复现，调试依赖 Chrome/Edge WebGPU、浏览器日志和截图回归。
