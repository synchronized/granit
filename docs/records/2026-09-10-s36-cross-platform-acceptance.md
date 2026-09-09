<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-10 S-36 跨平台验收

## 结论

S-36F 的桌面与浏览器视觉验收以及 Windows、Linux、Emscripten 平台矩阵已经通过。示例目录、
Model Viewer 交互与加载、Vulkan/WebGPU ImGui 固定画面均达到 0.20.0 发布候选要求；S-36G 只剩
版本文档、最终同提交矩阵和不可变 Release Candidate 晋级。

## 验证结果

- Windows Clang Debug 共享库 83/83、静态库 72/72 测试通过；安装导出、C11/C++20 Consumer、
  Vulkan 1×/2× 固定画面以及 Win32 DPI、焦点、输入和 SDL3 窗口 Smoke 均通过。
- [Windows](https://github.com/synchronized/granit/actions/runs/34376406135)完成 VS2022 共享/静态、
  安装 Consumer 和真实 Vulkan 示例验证。
- [Linux](https://github.com/synchronized/granit/actions/runs/34376886643)完成 GCC/Clang 共享/静态、
  安装 Consumer 以及 X11/Wayland SDL3 + ImGui 集成验证。
- [Emscripten](https://github.com/synchronized/granit/actions/runs/34409267972)完成静态 SDK、独立
  Consumer、产物审计，并在独立 Runner 上验证平台 Smoke、正式 Model Viewer 和 ImGui 1×/2×
  DPI 页面行为。
- [Quick Check](https://github.com/synchronized/granit/actions/runs/34376400312)和
  [Documentation](https://github.com/synchronized/granit/actions/runs/34376420390)通过。

## 验收边界

- 视觉门禁检查固定语义区域的颜色、纹理、裁剪、字体覆盖和交互状态；整图作为诊断产物保存，
  不要求不同驱动和字体栅格实现逐像素相同。
- GitHub Hosted Linux Chrome 不暴露 WebGPU Canvas 合成像素时，浏览器测试明确跳过颜色采样，
  继续验证 1×/2× 页面帧推进、点击状态、Draw Data、输入、Resize 和资源关闭；Vulkan 离屏像素
  仍是固定画面的强制视觉门禁。
- Emscripten 的三项浏览器验收分别运行于独立 Runner，避免软件 WebGPU 外部实例的进程级状态
  串扰；已确认的设备丢失或透明 Canvas 合成可以重试，其他视觉差异立即失败。
- 以上运行覆盖功能提交及随后对应平台的 CI 修复。0.20.0 发布收尾会在最终发布准备提交上重新
  运行完整工作流，并在发布记录中保存最终结果。
