<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 07：Deferred Rendering

本教程把实例化立方体先写入三张 G-buffer，再用全屏 Lighting Pass 合成到交换链，展示 Granit
如何用现有公共接口表达 MRT、中间纹理采样和多 Pass 渲染。完整源码位于
[`examples/tutorials/07_deferred`](../../examples/tutorials/07_deferred)。

## G-buffer 与 Pass

Geometry Pass 同时输出 View-space Position/Depth、Normal 和 Albedo/Material，并使用独立 Depth
Attachment 完成遮挡。前三张纹理都带 `color_attachment | sampled` Usage，因此 Pass 结束后能由
Lighting Pass 直接采样：

```text
Instanced Cubes
      ↓ Geometry Pass
Position/Depth + Normal + Albedo/Material + Depth
      ↓ Lighting Pass
Backbuffer
      ↓ Canvas / ImGui
Present
```

Position 和 Normal 使用 `rgba16_float`，Albedo 使用 `rgba8_unorm`，Depth 使用 `d32_float`。初始化
先查询 Renderer 的最大颜色附件数量和格式 Usage；共同基线不可用时返回 `unsupported`，不会进入
后端专用替代路径。

Lighting Pass 绘制一个全屏三角形，循环计算 1～64 个点光源。光源数量和数据只通过动态 Uniform
更新，调整数量不会重建 Pipeline 或增加 Draw。面板可以切换 Final Lighting、Position/Depth、
Normal 和 Albedo 输出，以直接检查每张中间附件。

## 生命周期与跨后端行为

G-buffer Texture、View 和 Lighting Bind Group 跟随当前交换链尺寸。Resize 时按依赖顺序销毁 Bind
Group、View 和 Texture，再为新尺寸完整重建；零尺寸由 Application 的 Swapchain 状态机暂停绘制。
每帧仍由一个 Frame Recording 顺序录制 Geometry、Lighting 和 Canvas，资源状态转换由 Recorder
处理，不向教程暴露 Vulkan Barrier 或 WebGPU Pass 类型。

[`deferred.hlsl`](../../examples/tutorials/07_deferred/deferred.hlsl) 包含 Geometry 与 Lighting 的四个
入口。Shader Toolchain 从同一 HLSL-first 清单生成 SPIR-V 和 WGSL，桌面 Vulkan 与浏览器 WebGPU
编译同一个 `main.cpp`。

## 构建与验证

```powershell
cmake --preset windows-clang-debug
cmake --build --preset windows-clang-debug --target granit_tutorial_07_deferred
ctest --test-dir build/windows-clang-debug -R "^granit\.tutorial\.07_deferred$" `
  --output-on-failure
```

Smoke 模式固定相机、动画和光源，并轮换四种输出模式。浏览器验收检查三个 Pass、活动光源、输出
模式切换、多帧推进、Canvas 内容、Resize 恢复及 WebGPU Validation 消息。
