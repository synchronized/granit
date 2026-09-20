<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-47：Window 跨平台入口收敛

## 状态

**已完成。** Win32、XCB、Wayland 与 Emscripten 现在共享 Window System、Window、事件轮询、
输入状态和 Surface 创建入口。浏览器 Model Viewer 已迁移到公共 Window component。

## 背景与目标

桌面平台已经通过 Window component 隔离原生窗口系统，但浏览器仍由示例直接注册 DOM 回调、读取
Canvas 尺寸并创建 Canvas Surface，形成第二套窗口和输入生命周期。本任务让浏览器使用与桌面
相同的公共调用流程，同时保留各平台内部实现差异。

目标：

- Emscripten 安装包提供 `granit::window`，公共头不暴露 DOM 或 Emscripten 类型。
- 四个平台统一使用 `process_events`、两个事件队列、状态查询和 Window Surface 创建。
- 平台输入解码共享按键状态与修饰键计算，平台锁定键和 keymap 逻辑留在后端。
- 浏览器 Smoke 覆盖输入、Canvas Resize、Surface、Swapchain、Present 与清理。

## 非目标

- 不抽象浏览器多标签页、弹出窗口或任意 DOM 元素。
- 不新增触摸、手写笔、IME 预编辑、Pointer Lock 或剪贴板协议。
- 不删除外部 SDL3、GLFW、Qt 使用的 Renderer 原生 Surface 高级入口。
- 不要求 Win32、XCB、Wayland 与浏览器产生平台本身不存在的事件。

## 已确认决策

- 公共 API 统一，平台实现继续位于 `src/window/platform/<platform>/`。
- Emscripten Window 绑定默认 `#canvas`，当前页面只允许一个活动 Granit Window。
- Canvas CSS 尺寸是逻辑窗口尺寸，Canvas 元素尺寸是 Framebuffer 尺寸；高 DPI 只决定初始比例，
  页面后续修改 Canvas 元素尺寸时由 `process_events` 同步。
- DOM 回调只负责转换并写入统一队列，应用仍按桌面相同顺序显式处理和轮询事件。
- 浏览器 Window 创建 Surface 时内部生成 Canvas 来源，普通使用者不接触 selector 或原生描述。

## 实施结果

1. 增加 Emscripten Window backend 枚举、系统与窗口生命周期、Canvas 状态同步和 Surface 创建。
2. 增加 DOM 键盘、文本、指针、滚轮与焦点转换，并补齐功能键、重复键和左右修饰键状态。
3. 抽取各平台共享的按键位图与修饰键辅助逻辑，Win32、XCB、Wayland 保留各自锁定键来源。
4. 浏览器安装包导出 Window component，安装 Consumer 验证 `granit::window`。
5. 浏览器 Model Viewer 删除私有 DOM/Canvas 路径，迁移到公共 Window 帧循环。

## 测试与验收

- Windows Clang Window、输入、C/C++ 头测试通过。
- Emscripten Debug 构建 Window、平台 Smoke 和 Model Viewer 通过。
- Chrome 验证 WebGPU 多帧、输入、Resize、资产加载和资源清理通过。
- Emscripten 安装 SDK 的 `RenderPipeline + Window` C Consumer 通过。
- Linux CI 继续验证 XCB、Wayland 编译和真实显示服务器行为。

## 当前限制

- 浏览器固定使用 `#canvas`，多 Canvas 需要后续通过通用窗口目标描述单独设计。
- 浏览器文本输入只覆盖已提交的 `keypress` 文本，IME 组合阶段尚未进入公共事件模型。
- 本地环境无法执行 Linux XCB/Wayland 构建，最终跨平台验收依赖 Linux CI。
