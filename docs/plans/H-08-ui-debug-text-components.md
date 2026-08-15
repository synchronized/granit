<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-08：公共 UI、Debug Draw 与 Text components

## 状态

- 优先级：P2
- 状态：进行中；H-08A Canvas Draw List 数据 ABI 已完成，下一步 H-08B
- 必需依赖：H-06 Unlit、2D 与 UI 内部技术验证

## 目标

把 H-06 验证过的内部能力逐步提升为职责独立的公共 C ABI，同时保持 Renderer 不感知控件、字体和
调试命令。长期边界遵循 [ADR-001](../decisions/ADR-001-debug-text-boundary.md)。

## 非目标

- 不实现控件树、布局、输入、主题或完整 UI 框架。
- 不让第三方 UI、字体整形或栅格化库成为核心 Renderer 的传递依赖。
- 不直接公开内部 Material、Upload Buffer 或 Render Graph 类型。

## 实施顺序

1. **H-08A：公共 Canvas Draw List 数据 ABI**——提供句柄、容量预留、clear、通用三角形、矩形
   便捷接口、状态借用和合批统计，并补齐 C/C++ 头文件与句柄测试。
2. **H-08B：录制与参考管线提交**——设计不泄漏内部 Material 的录制接口，使 Draw List 可进入独立
   Canvas Pass 和 Tone Mapping 后 Overlay 阶段；验证离屏、窗口、颜色空间和资源失效。
3. **H-08C：Debug Draw component**——定义当前帧线段、三角形和基础 Gizmo 命令，分别生成世界
   空间 Unlit 与屏幕空间 UI 几何。
4. **H-08D：Text component 原型**——定义已整形字形、Atlas 缓存和 UI 四边形边界，再评估可选的
   整形与栅格化依赖。
5. **H-08E：第三方适配验证**——至少以一种立即式 UI Draw Data 验证通用三角形入口，不把该库
   设为 Granit 必选依赖。

## H-08A 完成结果

- `.h` 与 `.hpp` 分别提供 C11 ABI 和轻量 C++20 RAII，不暴露内部容器。
- Canvas Draw List 使用 64 位 generation 句柄并校验所属 Renderer；clear 保留容量。
- 通用接口复制顶点和相对索引，矩形接口覆盖常见 Sprite/UI 图片输入。
- Texture View 与 Sampler 只被借用，状态相同的相邻 Item 合并为一个 Batch。
- 测试覆盖 C/C++ 独立包含、合批、复用、无效数据、跨 Renderer、重复销毁和旧句柄。

## 验收标准

- 普通使用者不接触 Vulkan、内部 Material 包或 Render Graph 实现类型。
- 每帧 UI 可通过少量 C ABI 调用批量构建并一次录制，避免逐控件跨 DLL 调用。
- Debug Draw、Text 和第三方 UI 只生成或消费公共 Canvas Draw List，不复制 Renderer 资源系统。
- 所有借用资源、线程安全、颜色空间和销毁顺序均有明确文档与测试。
