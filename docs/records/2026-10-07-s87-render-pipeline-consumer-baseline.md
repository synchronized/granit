<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-87A RenderPipeline Consumer 基线

## 结论

S-87A 的第一阶段基线已通过。仓库现有安装 Consumer 已经能够只通过安装导出的
`granit::render_pipeline` 目标，编译并运行 C11 与 C++20 Consumer；C++ Consumer 覆盖
RenderPipeline、Scene、Material、Mesh、实际输出纹理、最小 Render 调用、移动语义和环境图。
本阶段没有发现需要新增公共 API 的缺口。

## 验证范围

- C11 与 C++20 安装 Consumer 的头文件、链接目标和共享/静态链接属性；
- RenderPipeline 与 Core、Window component 的链接类型一致性；
- 无可用 Vulkan 后端时 Consumer 的合法失败路径；
- 可用后端时 Renderer、RenderPipeline、Scene、输出 Texture/View 和 Environment Map 的创建、
  使用、移动和销毁；
- Pipeline API、实际渲染集成、安装构建树、版本一致性、发布准备和 Vulkan Runtime Bundle。

## 命令与结果

在 Windows Clang Debug 构建树执行：

```text
ctest --test-dir build/windows-clang-debug -R "granit\.(pipeline|packaging)" --output-on-failure
```

结果：7/7 通过，包括 `granit.pipeline.render`、`granit.pipeline.api`、安装 Consumer 构建树、
发布版本检查、发布准备和 Runtime Bundle 检查。

## 当前差异

现有安装 Consumer 尚未把 Canvas、Text Draw List 和 Debug Draw 组合到同一个实际输出帧中；这些
能力已有独立 API/行为测试，下一阶段 S-87B 将补充它们与 RenderPipeline 的生命周期和录制边界
验收，不在本记录中把未完成项标记为通过。
