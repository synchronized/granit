<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.2 迁移到 0.3

0.3 保留 0.2 的公共 C 函数和 CMake component 名称，同时增加 Frame Context、Buffer flush、帧槽
查询和 Canvas 批量追加。Canvas 描述布局及部分失败结果分类发生了 0.x 阶段允许的破坏性变化，
使用者应重新编译并完成以下检查。

## 重新初始化 Canvas 描述

不要复制 0.2 的结构体字节、硬编码 `struct_size` 或用全零结构替代初始化宏：

```c
granit_canvas_draw_list_desc list_desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
list_desc.frame_slot_count = frames_in_flight;

granit_canvas_record_desc record_desc = GRANIT_CANVAS_RECORD_DESC_INIT;
record_desc.frame_slot = frame_slot;
```

`frame_slot_count` 使用原保留空间，但已经具备公开语义；`granit_canvas_record_desc` 新增
`frame_slot` 并扩大当前 V1 尺寸。使用旧头编译的 Canvas 描述不属于 0.3 二进制兼容输入。

窗口帧循环应从 `granit_frame_context_begin` 取得 `frame_slot`。调用方自行保证 GPU 同步的离屏路径
可以保留初始化宏提供的 `GRANIT_CANVAS_FRAME_SLOT_AUTO`。

## 更新错误分支

0.3 统一区分“参数形状错误”和“资源身份错误”：

- 空描述、空输出指针、非法范围或未知标志返回 `GRANIT_ERROR_INVALID_ARGUMENT`。
- 空句柄、失效句柄、资源类型错误或跨 Renderer/父对象混用返回
  `GRANIT_ERROR_INVALID_HANDLE`。
- C++ 空 RAII 对象的 `reset()` 仍幂等成功；若本地对象仍保存已被父对象级联失效的句柄，首次
  `reset()` 返回 `invalid_handle` 并清空本地状态。

如果旧代码把所有失败都归入 `INVALID_ARGUMENT`，应改为先判断 `granit_result`，再把诊断回调用于
日志定位；不要解析诊断文本实现分支逻辑。

## 采用 Frame Context

0.2 中由调用方为每帧手工创建、等待和重置 Command Recorder 的代码仍可使用，但窗口循环推荐
迁移到 Frame Context：

1. 按 Renderer 创建一个 `granit_frame_context`。
2. acquire Frame 后调用 `granit_frame_context_begin`，取得借用 Recorder 和真实帧槽。
3. 完成录制后调用 `granit_frame_context_submit`，随后 present。
4. 失败路径调用 `granit_frame_context_abort`；借用 Recorder 不得单独销毁。

C++ 可使用 `granit::frame_context` 和 `granit::frame_recording` 管理相同生命周期。详细状态约束见
[Frame Context 参考](../reference/frame-context.md)。

## 构建与包检查

- 继续使用 `Core`、`RenderPipeline`、`Window` 和 `Input` component；目标名保持
  `granit::granit`、`granit::render_pipeline`、`granit::window` 和 `granit::input`。
- 删除旧构建目录或至少重新运行 CMake 配置，确保使用 0.3 生成头和安装导出。
- 重新编译全部 C/C++ Consumer；不混用 0.2 头文件与 0.3 动态库。
- 运行项目提供的安装 Consumer CTest，验证链接模式和运行时库搜索路径。

0.3 仍属于未冻结的 0.x 版本。公共 C 导出没有删除或改名不代表所有描述布局、行为和 C++ 类布局
都具备跨次版本二进制兼容性。
