<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10E WebGPU Renderer 验收

## 结论

S-10E 已完成。Emscripten 构建现可通过 Granit 公共 Renderer、Surface、Swapchain、Shader、
Graphics Pipeline 和 Command Recorder API，在浏览器 Canvas 绘制确定性三角形；公共头文件不暴露
WebGPU、Emscripten 或 Vulkan 原生类型。Windows、Linux 与 Emscripten 最终矩阵均已通过。

## 验收范围

- 异步 WebGPU Provider 生命周期、状态查询、事件推进和失败传播。
- Canvas Surface、Swapchain、借用 Backbuffer、Acquire、Cancel 与 Present 生命周期。
- WGSL Shader、空 Pipeline Layout、单颜色附件 Graphics Pipeline 和三角形 Draw/Submit。
- 浏览器示例产物、Renderer 状态、键盘与鼠标输入及可读合成层像素。
- Vulkan 原生后端、共享/静态库、GCC/Clang/MSVC、安装导出和独立 C/C++ Consumer 回归。
- 公共 ABI 导出快照及 Mock Provider 随机测试顺序稳定性。

## 验证结果

- Windows Actions：MSVC 共享/静态构建、测试、安装及独立 Consumer 全部通过。
- Linux Actions：GCC/Clang 共享/静态及 SDL3/ImGui Integration Runtime 全部通过。
- Emscripten Actions：锁定 emsdk 5.0.6 的产物审计和无头 Chrome 公共绘制闭环通过。
- 本地 Windows Clang：浏览器示例、插件 Loader、完整测试、安装导出和 Consumer 验证通过。
- Mock Provider 使用 10 个随机种子重复执行，确认测试结果不依赖 Catch2 用例顺序。

最终验证运行：

- [Windows #33142507199](https://github.com/synchronized/granit/actions/runs/33142507199)
- [Linux #33143254661](https://github.com/synchronized/granit/actions/runs/33143254661)
- [Emscripten #33142924824](https://github.com/synchronized/granit/actions/runs/33142924824)

## 验收中修正的问题

- 公共 Renderer 状态、事件推进和 Canvas Surface 新增符号已同步进入核心 ABI 导出快照。
- Mock Provider 不再用全局实例创建序号决定异步结果，避免随机测试顺序造成 GCC 偶发失败。
- Linux SwiftShader 可能无法把 WebGPU Canvas 合成结果暴露给页面截图；测试会明确记录透明中心像素，
  并继续验证状态、命令和输入。确定性颜色结果由同一插件命令路径的离屏回读测试覆盖。

## 剩余边界

0.4.0 不提供桌面公共后端选择枚举、WebGPU 原生互操作、浏览器多线程渲染或 Android 平台集成。
这些能力需要分别经过能力模型和产品需求评估，不属于 S-10E 验收范围。
