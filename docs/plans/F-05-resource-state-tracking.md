<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# F-05：资源状态跟踪与屏障

## 元数据

- 设计状态：已确认
- 实现状态：F-05A 已完成；F-05B 待开始
- 路线图任务：F-05
- 优先级：P0
- 前置依赖：F-02、F-04、R-02、R-09
- 后续依赖：F-06、D-04

## 核心约束

公共 API 只表达 copy source、copy destination、color attachment 等后端无关访问意图，不公开
Vulkan Pipeline Stage、Access Mask 或 Image Layout。Granit 根据命令顺序生成同步信息。

不同 Recorder 可以并行录制，因此不能在录制阶段直接修改 Texture 上唯一的“全局当前 Layout”。
录制顺序、线程完成顺序和 Queue 提交顺序并不等价；这样做会让后录制但先提交的 Command Buffer
使用错误的 `oldLayout`。Texture 状态必须在提交顺序确定后解析，或使用能够延迟修补的状态批次。

## F-05A：Buffer 访问跟踪（已完成）

Recorder 为使用过的 Buffer 保存最近一次 Stage/Access：

- copy source → `TRANSFER_READ`；
- copy destination 与 fill destination → `TRANSFER_WRITE`；
- 同一命令中相同 Buffer 的访问合并后再生成屏障；
- 纯 read-after-read 不生成无意义屏障；
- 首次访问使用保守的 `ALL_COMMANDS + MEMORY_READ/WRITE` 来源，覆盖同一 graphics Queue 上更早
  提交对该 Buffer 的潜在访问；
- reset 和 destroy 清空 Recorder 局部状态。

实际命令通过 Vulkan 1.3 `vkCmdPipelineBarrier2` 和 `VkBufferMemoryBarrier2` 建立依赖。第一版按完整
Buffer 范围同步；以后获得真实热点数据后，再评估区间状态拆分。

## F-05B：Texture Layout 与访问状态（待开始）

需要覆盖：

- `UNDEFINED` 到首次有效用途；
- color/depth-stencil attachment 的读写与 Layout；
- store 后再次 load 的内容保留；
- transfer、sampled、storage 和 present 状态；
- 同一 Texture 不同 mip/layer/aspect 的子资源状态；
- 多 Recorder 并行录制后按实际提交顺序解析状态；
- Swapchain acquire 提供的初始状态和 present 目标状态。

第一版可以按整张 Texture 跟踪，但数据结构必须允许以后拆分子资源。不得把所有 Image 永久固定在
`GENERAL` Layout 来回避状态设计，也不得每次从 `UNDEFINED` 转换而丢弃需要保留的内容。

## 线程与生命周期

- Recorder 互斥锁保护其局部访问记录和 `vkCmdPipelineBarrier2` 录制。
- Queue 互斥锁负责按提交顺序合并最终资源状态。
- Registry 锁只获取稳定资源引用，不参与同步状态求解。
- Recorder 保留所有被屏障和命令引用的资源，直到提交完成后的 reset 或 destroy。

## F-05A 验收结果

- 连续 fill、copy 和 copy 后 fill 会生成正确的 transfer 内存依赖。
- 同 Buffer 的非重叠 copy 访问合并为一个 read/write 目标状态。
- reset 后旧的 Recorder 局部状态被清空。
- 设备初始化要求 `vkCmdPipelineBarrier2`。
- Clang 动态库与 MSVC 静态库在严格警告下构建和测试通过。
