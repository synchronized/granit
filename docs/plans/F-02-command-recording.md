<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-02：基础命令录制

## 元数据

- 设计状态：已确认
- 实现状态：F-02A 已完成；F-02B 等待实现
- 路线图任务：F-02
- 优先级：P0
- 前置依赖：F-01、R-09
- 后续依赖：F-03、F-04、F-05、D-04

## 分阶段范围

### F-02A：Buffer 命令

- 多区域 Buffer Copy。
- 32 位模式 Fill Buffer。
- usage、范围、对齐和同 Buffer 重叠校验。
- Recorder 保存参与命令的资源内部强引用。
- reset 或 destroy 时释放未提交录制的引用。

### F-02B：Dynamic Rendering

- `begin_rendering` / `end_rendering`。
- 接入 R-09 颜色和深度/模板 Attachment。
- 渲染区域、附件数量、格式、用途、尺寸、采样数和 Renderer domain 校验。
- 禁止渲染作用域嵌套，以及在作用域中录制不允许的复制命令。
- Attachment load-clear 作为第一版颜色和深度清理方式。

### F-02C：随 F-05 接入状态

- Recorder 记录 transfer read/write 和 attachment read/write 访问意图。
- F-05 将局部访问合并为实际 Vulkan barrier 和 Layout 转换。
- 独立 Texture Clear 等依赖状态转换的命令在此后评估。

## Buffer Copy API

```c
granit_command_recorder_copy_buffer(
    renderer, recorder, source, destination, regions, region_count);
```

一次调用接受多个 `granit_buffer_copy_region`。区域结构只包含 64 位 source offset、destination
offset 和 size，ABI 大小为 24 字节。数组只在调用期间读取。

校验规则：

- Recorder 必须处于 recording。
- Source 和 Destination 必须属于 Recorder 的 Renderer。
- Source 必须含 `TRANSFER_SOURCE` usage。
- Destination 必须含 `TRANSFER_DESTINATION` usage。
- region 指针非空、数量非零，每个 size 非零且范围不越界。
- 同一个 Buffer 内复制时，全部 source 范围与全部 destination 范围不得重叠。

第一版使用 `vkCmdCopyBuffer`。接口已经批量化，不为每个区域增加一次动态库调用。

## Fill Buffer API

```c
granit_command_recorder_fill_buffer(
    renderer, recorder, buffer, offset, size, value);
```

Buffer 必须含 `TRANSFER_DESTINATION` usage；offset 和 size 必须是 4 的倍数，size 非零且范围有效。
第一版不公开 Vulkan 的 `VK_WHOLE_SIZE` 特殊值，调用者提供明确范围。

## 资源强引用

命令录制前通过 Handle Table 校验公开句柄，然后 Recorder 保存去重后的内部强引用。用户可以在
end 或 submit 前销毁 Buffer 公开句柄，底层 allocation 仍存活。

F-02A 没有提交能力，因此成功 reset 或 destroy 会释放全部引用。F-04 接入提交后，引用必须转移
到 pending 批次并保留至对应提交序号完成，再由 R-08B 回收已退役资源。

第一版使用线性去重数组。每个 Recorder 的资源数量有实际数据后，再评估是否改为哈希结构。

## 锁与错误

- Registry 锁只用于取得 Recorder 和 Buffer 的稳定内部引用。
- 范围转换和 Vulkan 命令录制在 Registry 锁外完成。
- Recorder 互斥锁覆盖状态检查、引用集合和 `vkCmd*` 调用。
- 分配资源引用或区域数组失败返回 `GRANIT_ERROR_OUT_OF_MEMORY`。
- Vulkan `vkCmdCopyBuffer` 和 `vkCmdFillBuffer` 为 void，前置验证失败时不录制部分命令。

## F-02B 预定语义

开始渲染描述将包含明确渲染区域、颜色 Attachment 指针与数量、可空深度/模板 Attachment 以及
固定为 1 的 layer 数量。底层使用 Vulkan 1.3 Dynamic Rendering，不建立 RenderPass 或
Framebuffer 缓存。

公共 API 不接收 Vulkan Layout、Stage 或 Access Mask。F-02B 先建立结构和访问意图，F-05 再
完成自动状态转换；在 F-05 前不宣称支持任意跨命令同步。

## 测试与结果

F-02A 已覆盖：

- recording 状态要求。
- 多区域复制和 fill 命令录制。
- 资源公开句柄提前销毁后 Recorder 仍能安全 end/reset。
- 越界、非 4 字节对齐和同 Buffer 重叠被拒绝。
- C/C++ 公共入口和两种链接方式构建。

F-02B 完成后补充离屏颜色清除、深度清除、Attachment 交叉校验和渲染作用域状态测试。

Windows Clang + Ninja Debug 动态库和 Visual Studio 2022 Debug 静态库均在严格警告下构建，
全部测试通过。
