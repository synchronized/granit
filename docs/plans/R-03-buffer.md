<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-03：Buffer 生命周期与映射

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-03
- 优先级：P0
- 前置依赖：R-01、R-02
- 后续依赖：R-04、R-08、F-01

## 目标与边界

本任务实现不带初始数据的 Buffer 创建、销毁和 CPU 映射闭环。初始数据、device-local 上传命令
及批量上传由 R-04 负责；GPU 使用期间的安全延迟释放由 R-08 负责。

Buffer 是 64 位整数句柄，校验资源类型、generation 和 Renderer domain。VMA、`VkBuffer`、
映射策略和缓存一致性仍是内部细节。

## 已确认接口

```c
granit_result granit_buffer_create(
    granit_renderer renderer,
    const granit_buffer_desc* desc,
    granit_buffer* buffer);

granit_result granit_buffer_map(
    granit_renderer renderer,
    granit_buffer buffer,
    uint64_t offset,
    uint64_t size,
    void** data);

granit_result granit_buffer_unmap(granit_renderer renderer, granit_buffer buffer);
granit_result granit_buffer_destroy(granit_renderer renderer, granit_buffer buffer);
```

创建不携带初始数据，避免隐式 Queue 提交和同步等待。所有操作显式接收 Renderer，以便在 C ABI
入口校验资源归属，并与现有 Surface、Swapchain 风格一致。

C++20 层提供 move-only `granit::buffer`。包装只保存 Renderer 和 Buffer 两个 C 句柄，析构调用
C API，不建立平行运行时状态。

## 映射语义

- 仅 `UPLOAD` 和 `READBACK` Buffer 可以映射。
- `DEVICE` 和 `AUTOMATIC` 返回 `GRANIT_ERROR_UNSUPPORTED`，即使 UMA 设备实际可映射。
- offset 和 size 使用字节，size 必须非零且范围不能越界。
- 指针只在成功 map 到对应 unmap 之间有效。
- 同一 Buffer 不允许嵌套映射；未映射时调用 unmap 返回非法参数。
- `READBACK` 在 map 成功前自动 invalidate。
- `UPLOAD` 在 unmap 时自动 flush 本次映射范围。
- 内部可以使用 VMA 持久映射，但这不构成公共 API 承诺。

显式范围 flush/invalidate 暂不公开。后续若性能测试需要部分访问控制，可追加独立 API，不改变
现有自动行为。

## 生命周期与线程安全

- 创建成功后 Buffer 由 Renderer 拥有，不能跨 Renderer 操作。
- destroy 先移除句柄，再在全局 Registry 锁外释放 VMA allocation。
- 映射期间显式 destroy 返回非法参数，调用者必须先 unmap。
- Renderer 销毁会级联使所有 Buffer 句柄失效并释放底层对象。
- 不同 Buffer 可以由不同线程并行操作。
- 同一 Buffer 的 map、unmap 和 destroy 不允许调用者并发；内部记录级互斥锁保护映射状态。
- 当前尚无异步 GPU 命令使用 Buffer，因此立即释放是安全的；R-08 后切换为延迟释放。

## 错误规则

- 空描述、空输出指针、零大小、空用途、未知用途位或非法内存位置返回非法参数。
- 零句柄、错误类型、旧 generation 或跨 Renderer 句柄返回无效句柄。
- 不允许映射的内存位置返回不支持。
- 嵌套映射、非法范围、未映射时 unmap 或映射期间 destroy 返回非法参数。
- VMA 内存不足和设备丢失继续映射为现有稳定结果码。

## 实现结果

- 新增 `buffer.h/.hpp`，并加入聚合头和安装 `FILE_SET`。
- Renderer Registry 新增 Buffer 记录与句柄表接入，Renderer 销毁支持级联清理。
- Buffer usage 转换、VMA 分配、映射状态、自动 flush/invalidate 均位于内部实现。
- C 测试覆盖入口参数；C++/内部测试覆盖真实创建、映射、范围错误、跨 Renderer、重复销毁、
  move-only RAII 和 Renderer 级联失效。
- R-03 没有实现初始数据或 device-local 上传，下一步进入 R-04。

## 验收标准

- C11 和 C++20 头文件能够独立包含。
- 共享库与静态库构建、测试和安装验证通过。
- 公共头文件不出现 Vulkan、Volk 或 VMA 类型。
- upload/readback 映射自动维护缓存一致性。
- 错误 Renderer、旧 generation、嵌套映射和重复销毁均被拒绝。
