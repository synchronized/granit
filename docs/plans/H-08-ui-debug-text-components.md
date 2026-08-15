<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-08：公共 UI、Debug Draw 与 Text components

## 状态

- 优先级：P2
- 状态：进行中；H-08A、H-08B 与 H-08C 已完成
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

## H-08B 当前结果

- H-08B1 提供公共 `granit_canvas_draw_list_record`，一次调用把完整列表录制到已有 Recorder。
- 默认坐标为左上原点、Y 轴向下的像素单位，内部持有 Material 与动态几何上传资源。
- 调用方仍负责 Recorder 的 begin、end、提交以及颜色目标生命周期；接口不泄漏内部 Material。
- H-08B2 已把 Draw List 作为单 View 或每个多 View 输出的可选 Canvas 输入接入参考管线。
- 自动路径先录制 Canvas，再调用用户 Overlay 回调；UNORM 输出自动使用 Shader sRGB 编码，sRGB
  Attachment 则交由硬件编码。

## H-08C 当前结果

- H-08C1 提供独立 `debug_draw_list` C ABI 与 C++ RAII 包装，批量保存线段和三角形命令。
- 命令明确区分世界空间与屏幕空间、深度测试模式、像素线宽和逐顶点颜色。
- 列表使用 Renderer domain、generation 句柄、容量复用及调用方外部同步，不进入底层 Renderer 状态。
- H-08C2 已按稳定命令顺序将屏幕线段展开为四边形、复制屏幕三角形，并一次追加到 Canvas。
- 内部白纹理与 Sampler 由 Debug Draw List 懒创建；下一步 H-08C3 处理世界空间 Unlit 录制。
- H-08C3a 已验证 Vulkan 齐次裁剪体内的线段裁剪、近面交点颜色插值和按视口像素展开。
- 下一步 H-08C3b 建立专用世界调试 Shader、深度分组和公共录制接口。
- H-08C3b1 已固定零 Bind Group 的 clip-space 顶点 Shader，以及线性输出和 Shader sRGB 编码
  Fragment 变体。
- H-08C3b2 已提供世界命令批量录制接口，并按颜色格式、深度格式、深度模式和 sRGB 编码懒缓存
  Pipeline；动态顶点 Buffer 按需扩容并复用，稳定命令顺序只在深度模式切换处形成 Draw Batch。
- 像素回读覆盖世界三角形颜色输出，深度测试覆盖遮挡行为。
- H-08C3c 已允许单 View 简写和多 View 独立输出绑定世界 Debug Draw List；自动路径在 Tone Mapping
  后复用当前 View 深度录制世界命令，再依次录制 Canvas 和用户 Overlay 回调。
- 多 View 回读验证未绑定列表的输出不受影响，H-08C 至此闭合；下一步进入 H-08D Text component。

## H-08D 当前结果

- [ADR-002](../decisions/ADR-002-text-input-boundary.md) 已固定“调用方整形和栅格化、Granit 管理 Atlas
  与 Canvas 几何”的边界，首版不引入强制字体第三方库。
- H-08D1 提供独立 `text_draw_list` C ABI 与 C++ RAII 包装，批量复制已定位字形和 Run Scissor。
- 字形以调用方 `font_key`、字体内 `glyph_id`、基线坐标和 RGBA8 颜色表达；列表支持容量复用、
  Renderer domain 与 generation 失效检查。
- 下一步 H-08D2 设计调用方位图上传、R8 Atlas 分页和字形缓存接口，再生成 Canvas 四边形。

## 验收标准

- 普通使用者不接触 Vulkan、内部 Material 包或 Render Graph 实现类型。
- 每帧 UI 可通过少量 C ABI 调用批量构建并一次录制，避免逐控件跨 DLL 调用。
- Debug Draw、Text 和第三方 UI 只生成或消费公共 Canvas Draw List，不复制 Renderer 资源系统。
- 所有借用资源、线程安全、颜色空间和销毁顺序均有明确文档与测试。
