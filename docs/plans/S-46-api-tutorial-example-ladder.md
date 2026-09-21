<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-46：API 教程与可运行示例阶梯

## 状态

**实施中。** 学习路径和示例边界已确认；`minimal_renderer`、`triangle` 及对应教程已经落地。

## 背景与目标

当前 `docs/tutorials/` 只有完整目标教程，`examples/` 只有 ImGui 和 Model Viewer 两类完整应用，
缺少从 Renderer、资源、Shader、Pipeline 到 Render Pipeline 的连续学习入口。若直接在教程中增加大量
代码，容易与完整示例重复；若只增加独立 API 示例，又会让 `examples/` 变成测试程序集合。

本计划建立一条由底层到高层的 C++20 API 学习路径，并保持三类内容职责清晰：

- Tutorial 解释连续目标、关键代码和预期结果；
- Example 提供可编译、可运行、可验证的完整程序；
- Reference 继续作为 API、格式、所有权和行为的唯一权威来源。

## 非目标

- 不修改公共 API、ABI、资源格式或运行时语义。
- 不把 `tests/smoke` 改造成面向使用者的示例。
- 不为每个 API 创建一个独立可执行文件。
- 不复制 Model Viewer 的完整实现到教程正文。
- 不在本计划中设计公共 glTF 加载器或新的示例共享 SDK。
- 不在基础教程完成前承诺 C API 与 C++ API 的完整双轨教程。

## 已确认决策

### 目录命名

Tutorial 按学习顺序使用数字前缀；Example 使用表达场景目标的语义名称，不使用数字前缀：

```text
docs/tutorials/
├─ 01-first-renderer.md
├─ 02-resources-and-upload.md
├─ 03-shader-and-pipeline.md
├─ 04-window-and-frame-loop.md
├─ 05-render-pipeline-offscreen.md
└─ 06-model-viewer.md

examples/samples/
├─ minimal_renderer/
├─ triangle/
├─ imgui/
└─ model_viewer/
```

只有对应内容和可运行目标实际落地后，才为现有 Tutorial 增加数字前缀，避免出现编号断档。

### 示例边界

- `minimal_renderer` 只展示 Renderer 创建、能力查询、错误处理和最小生命周期。
- `triangle` 展示资源、上传、Shader、Pipeline、Command Recorder 和一次 Draw。
- `imgui` 展示 Window、SDL3、ImGui 和 Canvas 集成。
- `model_viewer` 展示完整高层应用，不作为基础 API 的唯一学习入口。

单个错误路径、边界条件和回归行为仍由 `tests` 覆盖，不为教学目的复制成示例程序。

### 代码归属

教程保留帮助理解当前步骤的最小代码片段；完整可编译程序位于 `examples/`。教程可以引用示例中的
文件、目标和运行命令，但不复制会独立演化的完整实现。

## 实施顺序

1. **S-46A 路径与目标设计（已完成）**：确认四个基础阶段的 API 依赖、示例目标名称、构建选项和
   跨平台范围。
2. **S-46B `minimal_renderer`（已完成）**：新增最小 C++ Consumer 示例，验证动态库定位和生命周期。
3. **S-46C `triangle`（已完成）**：新增无第三方窗口依赖的最小 GPU 渲染示例，覆盖 Shader、Pipeline、
   离屏绘制和回读验证。
4. **S-46D 基础教程（进行中）**：已完成 `01`～`03` 的 Renderer、资源上传和 Shader/Pipeline
   说明；后续补充可直接运行的资源上传步骤，并为每篇教程链接对应 Reference 和完整示例。
5. **S-46E 窗口与高层教程（已完成）**：编写 `04`，整理现有 Render Pipeline 和 Model Viewer 为
   `05`、`06`，同步修复文档中心和交叉链接。
6. **S-46F 验收与维护**：补充 CMake/CTest 文档检查、示例 Smoke、安装 Consumer 验证和 Tutorial
   阅读路径检查。

每个阶段应形成可独立评审的提交；不为不存在的示例预先创建空教程或空目录。

## 测试与验收

- 每个面向使用者的示例都能通过对应 preset 构建，并能在支持的平台运行或明确报告不可用原因。
- 示例不进入 Granit 安装导出，不传播第三方依赖，不包含 Vulkan 公共头依赖。
- `minimal_renderer` 覆盖成功初始化、能力查询、结果码错误和正常销毁。
- `triangle` 覆盖资源创建、上传、Pipeline 创建、提交和输出验证；失败路径由测试覆盖。
- Tutorial 中的命令、目标名、安装路径和 Reference 链接通过文档检查。
- C++ 教程示例不泄漏 Vulkan 类型；C API 教程若后续加入，另行定义 C11 验收范围。
- 运行 `git diff --check`、`cmake/check_documentation.cmake` 和相关 `granit.documentation` 测试。

## 风险与未决问题

- 当前公共 API 是否足以构建无窗口 `triangle` 示例，需要先核对 Render Target、Shader 资产和回读路径。
- 示例是否需要平台分支，取决于现有 shared/static 安装 Consumer 在 Windows 和 Linux 的可复现程度。
- 若最小示例必须依赖测试专用 Shader 或内部辅助代码，应先补齐公开的资产构建入口，不能直接暴露
  `tests` 或 `examples/common` 私有实现。
- 示例数量、C API 教程和浏览器教程是否扩展到同一阶梯，待基础 C++ 路径验收后再决定。
