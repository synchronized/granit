<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-10：动态 Uniform Buffer Offset

## 状态

- 设计状态：已确认
- 实现状态：进行中；D-10A 至 D-10C 已完成，下一阶段为 D-10D 测试与示例
- 路线图任务：D-10
- 优先级：P1
- 前置依赖：D-03、D-07、F-10

## 背景与目标

当前不可变 Bind Group 只能在创建时固定 Buffer Offset。逐对象更新 Uniform 时，调用方需要频繁
创建 Bind Group，无法高效实现每帧 Uniform Arena。D-10 增加后端无关的动态 Uniform Buffer
Offset，使一个 Bind Group 能在多次 Draw 或 Dispatch 时选择同一 Buffer 中的不同对齐区段。

目标包括：

- 增加 `dynamic_uniform_buffer` Binding 类型。
- Bind Group 创建时保存 Buffer、基础 Offset 和 Range，绑定命令提供动态 Offset 数组。
- Graphics 与 Compute 使用相同的动态 Offset 规则和验证实现。
- Vulkan 映射到动态 Uniform Descriptor，并为未来 WebGPU 动态 Offset 保持统一内部契约。
- 用一个 Uniform Buffer 中的两组变换完成真实绘制验收。

## 非目标

- 不引入 Mesh、资产格式、RID、Camera、Transform 或 Material 等上层对象。
- 不同时实现动态 Storage Buffer、Bindless、实例化、间接绘制或 Render Graph 改造。
- 不让 Granit 管理调用方的 Uniform Arena 分配策略或每帧对象缓存。
- 不在本任务迁移 WebGPU 公共 Renderer；只保证后端契约可映射到 WebGPU。

## 已确认决策

### Binding 与 Offset 语义

- `dynamic_uniform_buffer` 是 Bind Group Layout 的独立 Binding 类型，普通 `uniform_buffer` 行为不变。
- Bind Group Entry 中的 Offset 是基础 Offset，Range 是单个动态区段的有效范围。
- 有效地址由 `base_offset + dynamic_offset` 得出；Range 不随动态 Offset 改变。
- 动态 Offset 使用 `uint32_t`，基础 Offset、Range 和 Buffer Size 保持 64 位。
- 每次绑定传入的 Offset 数量必须与本次全部 Bind Group 中的动态 Binding 数量完全一致。
- Offset 顺序先按 Bind Group 数组顺序，再按各 Layout 的 `binding` 数值升序排列；数组 Binding
  首版不与动态 Uniform Buffer 组合，避免元素展开顺序含糊。

### 公共 API 与 ABI

- Graphics 和 Compute 的 Bind Group 绑定命令改为接收版本化描述结构，结构包含 `struct_size`、
  `first_group`、Bind Group 数组和动态 Offset 数组。
- 项目公共 ABI 尚未稳定，本任务直接迁移现有绑定函数及所有调用点，不保留旧签名兼容分支。
- C++20 包装接受 `std::span`，只做边界转换；Recorder 在调用期复制数组，不保存调用方指针。
- 公共接口不暴露 Vulkan/WebGPU 类型，也不传播任何后端头文件或链接依赖。

### 后端映射

- Vulkan Layout 与 Descriptor Write 使用 `VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC`，绑定时将
  动态 Offset 传给 `vkCmdBindDescriptorSets`。
- 内部 Recorder/Backend 契约显式接收动态 Offset Span，不把 Offset 烘焙进 Vulkan 专用对象。
- 后续 WebGPU 可映射为 Layout 的 `hasDynamicOffset = true` 和 `setBindGroup` 的动态 Offset 数组，
  D-10 不要求提前接通该公共运行路径。

## 实施顺序

1. **D-10A 公共契约**：已增加 Binding 类型和版本化绑定描述，同步 C API、C++ 包装、公共头文件
   编译测试及现有 Graphics/Compute 调用点；后端接通前非空动态 Offset 明确返回“不支持”。
2. **D-10B 注册表与验证**：已按每组 `binding` 升序记录动态 Uniform 元数据，并按 Bind Group
   数组顺序拼接；统一校验 Offset 数量、设备对齐、有效 Range 和溢出。Buffer Usage、基础 Offset、
   最大 Range 与资源归属复用 Bind Group 创建校验。
3. **D-10C Vulkan 后端**：已接入动态 Descriptor 类型、Pool 容量和 Graphics/Compute 命令绑定，
   Offset 在公共 API 校验后按原顺序传入 `vkCmdBindDescriptorSets`；公共“不支持”门禁已解除。
4. **D-10D 测试与示例**：补齐单元测试、Validation Layer 回归及双对象动态变换 smoke test，更新
   API 参考和使用指南。
5. **D-10E 跨平台验收**：运行 Windows/Linux、共享/静态、C/C++ Consumer 和安装导出矩阵。

## 测试与验收

- 覆盖单个和多个 Bind Group、多个动态 Binding，以及普通/动态 Binding 混合排列。
- 覆盖 Offset 数量不足或过多、未对齐、基础范围越界、动态范围越界和整数溢出。
- 覆盖错误 Buffer Usage、错误资源类型、跨 Renderer、旧 generation 和销毁后使用。
- Graphics 与 Compute 对相同布局使用一致的 Offset 顺序和错误语义。
- 一个 Uniform Buffer 保存两组变换，通过不同动态 Offset 绘制两个位置可区分的对象并回读像素。
- Vulkan Validation Layer 不报告 Descriptor、Offset、Range 或生命周期错误。
- C11/C++20 公共头、共享/静态安装 Consumer 以及 Windows/Linux Actions 全部通过。

## 风险与未决问题

- 绑定描述签名会产生源码和 ABI 变更；0.x 阶段直接迁移，但必须在迁移说明中明确记录。
- 动态 Offset 数组是扁平序列，排序规则必须由测试锁定，不能依赖容器遍历或后端隐式顺序。
- 设备的 `minUniformBufferOffsetAlignment` 可能显著增加 Arena 空洞，分配策略由上层应用负责。
- 后续若增加动态 Storage Buffer，应复用同一描述和排序规则，不新增平行绑定命令。
