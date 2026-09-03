<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-03 S-13H/S-13I 跨后端验收

## 结论

S-13H 环境光照与 S-13I 渲染质量配置已完成。Vulkan 参考渲染、Linux Dawn WebGPU 图像比较、
Windows Dawn 基础集成，以及浏览器 Emscripten WebGPU 的真实 Chrome Fixture 均通过。

## 验收范围

- Vulkan Lavapipe 生成固定 FlightHelmet 参考图。
- Linux Dawn Vulkan 使用相同模型、GRENV 环境、PBR Shader 和质量配置完成截图比较。
- Linux Dawn 完成 FIFO/Immediate 与 UI 开/关的性能采样和 JSON 报告校验。
- Windows Dawn D3D12 完成插件加载、Shader 工具、Pipeline、Bind Group 和 Model Viewer 构建。
- Emscripten 在真实 Chrome WebGPU 中完成共享 Fixture、模型查看器、输入转发、质量配置重建和
  退出资源归零验证。

## 结果

- [Dawn Integration #33727414996](https://github.com/synchronized/granit/actions/runs/33727414996)
  全部通过。
- [Emscripten #33729910076](https://github.com/synchronized/granit/actions/runs/33729910076)
  全部通过。
- 本地 Windows Clang Debug 的 Model Viewer 相关目标构建通过，5 个相关测试全部通过。

排查期间尝试把 Cube 纹理改为逐面上传。该方式未解决托管 Windows D3D12 的 Device Lost，并会
导致浏览器 WebGPU 首帧丢失设备，因此最终恢复为一次写入六个数组层。Windows 托管 Runner 不再
运行完整 IBL 截图和性能采样；这些项目由 Linux Dawn 自动验收和真实 Windows GPU 手动验收覆盖。
