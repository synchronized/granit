<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# ADR-007：分离 Window Backend 与 Window Target

- 状态：已接受
- 日期：2026-09-24

## 背景

Window 当前用 Backend 表达平台实现，却把 Emscripten 的输出位置固定为 `#canvas`。这使多窗口
缺少独立身份，也容易在接入 SDL3 时把 SDL 实现选择、Canvas selector 和平台专用参数混入同一个
枚举或描述结构。SDL3 既可创建桌面顶层窗口，也可在 Emscripten 绑定 Canvas，因此它与 Canvas
不是同一维度。

## 决策

Window System 的 Backend 只决定窗口、事件和输入由哪个实现管理。Window 的 Target 单独描述
窗口绑定位置，使用可扩展的带 `struct_size` C 描述；首个明确 Target 类型是 Canvas selector，
默认类型保持平台自然行为。

原生平台与 SDL3 都实现同一套 Window API。SDL3 是 Window component 的可选编译后端，公共接口
不暴露 SDL 类型；现有 IntegrationSDL3 继续服务由应用拥有的外部 `SDL_Window`。浏览器 Canvas
由页面拥有，Window 只复制 selector 并绑定，不创建 DOM 元素。

## 影响

- 多 Canvas 不再依赖硬编码 ID，Window 的几何、输入、Surface 和原生查询共享同一目标身份。
- 可在不增加 `canvas_id`、`native_parent` 等平行字段的情况下扩展新 Target 类型。
- Backend 与 Target 的组合必须显式校验；不支持的组合返回错误，不能忽略 Target。
- 启用 SDL3 后端的构建会增加私有运行时依赖；静态链接时仍需由包配置传递最终链接依赖。
- 原生 Emscripten 多窗口要求事件按 Canvas 绑定，页面壳需使目标 Canvas 可聚焦。

## 替代方案

- 直接向 `granit_window_desc` 增加 `canvas_id`：实现简单，但把浏览器细节固化在通用 Window
  描述中，后续嵌入目标仍会继续堆字段。
- 只让 SDL3 生成唯一 Canvas ID：无法修复原生 Emscripten 后端，也让相同公共 API 因后端产生
  不同目标语义。
- 以 IntegrationSDL3 单独提供 SDL Window API：会形成与 `granit::window` 平行的生命周期、事件
  和输入系统，使用者仍需关心后端。
- 在运行时注册第三方 Window 驱动：最灵活，但需要公开稳定的驱动 ABI、回调表和跨模块所有权，
  对当前仅接入 SDL3 的需求过重。
