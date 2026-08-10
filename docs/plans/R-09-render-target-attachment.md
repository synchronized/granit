<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-09：统一 Render Target Attachment

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-09
- 优先级：P0
- 前置依赖：R-05、R-07、R-08A
- 后续依赖：F-02、F-05、F-06、D-04

## 背景

离屏 Texture View 和 Swapchain Backbuffer View 已使用同一种公开句柄表示。开始渲染前还需要
统一描述颜色、深度和模板附件的加载、清除及保存行为，避免窗口输出和离屏渲染形成两套 API。

R-09 只冻结 Attachment 值类型和验证规则。真正的 `begin_rendering` 命令由 F-02 提供，资源
状态转换由 F-05 接入，Swapchain acquire/present 由 F-06 接入。

## 已确认决策

- Attachment 只接收 Texture View，不直接接收 Texture。
- Swapchain Backbuffer 与普通离屏 View 使用同一套描述。
- 第一版最多支持 8 个颜色附件和 1 个深度/模板附件。
- 颜色、深度和模板分别表达 load/store 行为。
- 第一版支持 load、clear、discard，以及 store、discard。
- 颜色清除值使用四个 `float`，不限制在 `[0,1]`，以支持浮点 HDR 格式。
- 深度清除值必须位于 `[0,1]`，模板清除值使用 `uint32_t`。
- 第一版不提供 MSAA resolve 字段，后续只能在结构尾部追加。
- Layout、barrier 和 Vulkan attachment 类型全部由 Granit 内部管理。
- Attachment 数组只在命令记录调用期间借用，不保存调用者指针。

## 公共类型

新增独立公共头文件：

```text
include/granit/renderer/render_target.h
include/granit/renderer/render_target.hpp
```

C API 提供：

- `granit_attachment_load_operation`。
- `granit_attachment_store_operation`。
- `granit_clear_color_value`。
- `granit_clear_depth_stencil_value`。
- `granit_color_attachment_desc`。
- `granit_depth_stencil_attachment_desc`。

C++20 层提供对应强类型枚举和值结构，并可转换为 C ABI 描述。当前阶段不新增动态库导出函数。

## Load 行为

- `LOAD`：保留并读取渲染前已有内容；首次使用或内容未定义时属于调用错误。
- `CLEAR`：开始渲染时使用描述中的清除值初始化附件。
- `DISCARD`：不关心此前内容，渲染开始后的未写入区域内容未定义。

Load 零值为 `UNDEFINED`，验证时拒绝。初始化宏默认使用 `CLEAR`，避免新附件意外读取未初始化
内容。

## Store 行为

- `STORE`：渲染结束后保留内容，供采样、复制、读取或 present 使用。
- `DISCARD`：渲染结束后不保证内容，允许后端省略存储。

Store 零值为 `UNDEFINED`，验证时拒绝。初始化宏默认使用 `STORE`。Swapchain Backbuffer 在
present 前必须使用 `STORE`；F-06 负责结合实际目标验证。

## 颜色附件

每个颜色附件包含：

- 非空 Texture View 句柄。
- load/store 操作。
- RGBA 浮点清除值。
- `struct_size` 和保留字段。

F-02 接入时必须验证 View：

- 属于当前 Renderer domain。
- 指向颜色格式 Texture。
- Texture 含 `COLOR_ATTACHMENT` 用途。
- mip/layer 范围可作为单个渲染附件。
- 尺寸和采样数与同一 Render Target 的其他附件一致。

## 深度/模板附件

深度和模板共用一个 View，但各自拥有独立 load/store 操作。纯深度格式必须把模板操作设为
`DISCARD`；包含模板平面的格式才能使用模板 `LOAD`、`CLEAR` 或 `STORE`。

F-02 接入时必须验证 View 指向深度/模板格式，且 Texture 含
`DEPTH_STENCIL_ATTACHMENT` 用途。深度清除值必须为有限数且位于 `[0,1]`。

## Render Target 集合

F-02 的开始渲染描述将包含：

- `颜色附件指针 + 数量`，数量范围为 0 至 8。
- 可空的深度/模板附件指针。
- 明确的渲染区域。

至少需要一个颜色或深度/模板附件。数组和描述只在调用期间读取；Command Recorder 必须复制
值数据并取得 View 的内部稳定引用，不能长期保存调用者指针。

## MSAA 与 Resolve

第一版只允许单采样附件，因为当前 Texture 实现也只支持 `sample_count=1`。后续支持 MSAA 时：

- 多采样颜色 View 仍放在原 `view` 字段。
- Resolve View、Resolve Mode 只能追加到描述结构尾部。
- 不通过新增一套“MSAA Attachment”类型破坏统一模型。
- 所有参与同一渲染实例的附件采样数必须一致，Resolve 目标必须单采样。

## 状态与同步

公共接口不出现 Vulkan Layout。F-05 根据 Attachment 用途、load/store 和后续资源使用自动生成
必要状态转换。第一版不提供“用户指定旧 Layout”之类的逃生字段。

`LOAD` 表示需要保留逻辑内容，不等同于由用户提供具体 Layout。`DISCARD` 允许实现从未定义内容
开始，但仍需要正确的访问和 Layout 转换。

## ABI 与扩展

- 描述结构以 `struct_size` 开头。
- 字段使用定宽整数、整数句柄和普通浮点值。
- 保留字段必须为零。
- 新字段只能追加，不重排既有字段。
- 不使用 Vulkan `pNext` 风格扩展链。
- 第一版 C 结构固定为 48 字节，并由 C/C++ 头文件测试确认。

## 验证分层

当前纯值验证负责：

- `struct_size`、保留字段和非空 View。
- load/store 枚举范围。
- 清除值必须为有限数。
- 深度清除范围。

F-02 的 Registry 验证负责 View domain、格式、用途、尺寸、采样数、借用有效期以及同一目标内的
交叉约束。不要把需要查询资源记录的检查塞进纯值验证函数。

## 测试矩阵

- C11 头文件可以独立包含并初始化描述。
- C++20 头文件提供默认值、强类型枚举和 ABI 转换。
- 零初始化、未知枚举、保留字段和空 View 被拒绝。
- NaN、无穷颜色值和越界深度值被拒绝。
- load/clear/discard 与 store/discard 的合法组合通过。
- C 和 C++ 下结构尺寸保持一致。
- F-02 后补充格式、用途、domain、尺寸和多附件交叉验证。
- F-06 后补充 Swapchain Backbuffer 与 present store 验证。

## 验收标准

- 离屏 View 与 Swapchain Backbuffer 使用同一 Attachment 类型。
- 公共头文件不包含 Vulkan 类型或头文件。
- C ABI 可扩展且能由 C11 编译器独立包含。
- C++ 包装不维护第二套运行时状态。
- 第一版校验规则具有确定结果，不依赖 Vulkan 设备。
- F-02 可以直接复用值类型，不需要修改已发布字段。

## 实现结果

已完成：

- 新增可由 C11 独立包含的 `render_target.h` 和 C++20 `render_target.hpp`。
- 颜色与深度/模板 Attachment 第一版 ABI 均固定为 48 字节。
- C++ 包装提供强类型枚举、合理默认值和无状态 `native()` 转换。
- 纯值验证覆盖结构尺寸、保留字段、空 View、操作枚举及清除值范围。
- C/C++ 头文件编译测试和资源验证测试覆盖默认值及失败路径。

Windows Clang + Ninja Debug 动态库和 Visual Studio 2022 Debug 静态库均在严格警告下构建，
全部测试通过。格式、用途、domain 和多附件交叉验证按计划留给 F-02。
