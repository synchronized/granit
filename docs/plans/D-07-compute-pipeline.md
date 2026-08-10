<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# D-07：Compute Pipeline 与 Dispatch

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：D-07
- 优先级：P1
- 前置依赖：D-01、D-02、D-03、F-04、F-05
- 后续依赖：D-08、阶段六性能任务

## 目标

在不暴露 Vulkan 的前提下打通 Compute Shader、Pipeline、Bind Group、命令录制、资源状态转换和
Queue 提交。第一版专注直接 Dispatch，不同时引入间接 Dispatch、Push Constant、Pipeline Cache
或异步 Pipeline 编译。

## 公共接口

新增独立的 `granit_compute_pipeline` 句柄，不使用含义模糊的通用 Pipeline 句柄：

```c
typedef granit_handle granit_compute_pipeline;

typedef struct granit_compute_pipeline_desc {
  uint32_t struct_size;
  uint32_t reserved;
  granit_pipeline_layout layout;
  granit_shader compute_shader;
  uint64_t reserved_2;
} granit_compute_pipeline_desc;
```

Command Recorder 增加：

```c
granit_command_recorder_bind_compute_pipeline(...);
granit_command_recorder_bind_compute_groups(...);
granit_command_recorder_dispatch(..., uint32_t x, uint32_t y, uint32_t z);
```

C++20 层提供 move-only `compute_pipeline`，以及对应的 `bind_compute_pipeline`、
`bind_compute_groups` 和 `dispatch`。Pipeline Layout 与不可变 Bind Group 继续由 Graphics 和
Compute 共用，不建立平行资源模型。

## 状态与校验

- Compute Pipeline 只接受 `GRANIT_SHADER_STAGE_COMPUTE` Shader。
- Dispatch 的三个组数量都必须大于零，并校验设备限制。
- Dispatch 只能在 Command Recorder 的 recording 状态且 Dynamic Rendering 区域之外执行。
- Dispatch 前必须绑定 Compute Pipeline；Graphics Pipeline 状态不能替代 Compute Pipeline。
- Graphics 与 Compute Bind Group 使用各自的 Vulkan bind point，但复用相同 Layout 兼容校验。
- Recorder reset 后清除 Compute Pipeline 绑定状态。

## 资源访问与屏障

Bind Group Layout 已声明资源类型，但第一版还没有显式区分 Storage 资源只读或读写。D-07 采用
保守但正确的状态：

- Uniform Buffer：Compute Shader 只读。
- Storage Buffer：Compute Shader 读写。
- Sampled Texture 与 Sampler：Compute Shader 只读。
- Storage Texture：Compute Shader 读写，使用 GENERAL Layout。

绑定 Compute Bind Group 时解析其不可变资源列表，Recorder 在 Dispatch 前准备 Buffer/Image
访问状态并保持相关资源到提交完成。该策略可能产生比必要更多的写屏障，但不会要求用户接触
Vulkan stage/access/layout。未来可在 Bind Group Layout 增加只读 Storage 标志，且只在结构末尾
扩展。

## 分步实施

1. **D-07A / 已完成**：Compute Pipeline 句柄、创建销毁、C++ RAII 和生命周期验证。
2. **D-07B / 已完成**：Compute Pipeline/Bind Group 命令绑定、Dispatch 状态机和 Vulkan 命令。
3. **D-07C / 已完成**：Storage Buffer/Texture 自动访问状态、真实计算测试和最小 Compute
   示例。

## 最终实现

- Compute Bind Group 根据 Layout 可见阶段生成 Buffer 与 Image 的保守访问集合。
- Recorder 在每次 Dispatch 前准备 Compute Shader 访问，后续 Copy 等命令会自动生成跨阶段屏障。
- Storage Buffer 使用 Shader Storage Read/Write；Storage Texture 使用 GENERAL Layout。
- Sampled Texture 和 Uniform Buffer 保持只读访问。
- `granit_compute_example` 在 GPU 写入 16 个整数，复制至 Readback Buffer 后输出并验证结果。

## 测试与验收

- C11 与 C++20 公共头文件独立编译。
- 覆盖错误 Shader 阶段、错误 renderer、重复销毁、旧 generation 和跨 renderer 混用。
- 覆盖未绑定 Pipeline、Rendering 区域内 Dispatch、零组数量和不兼容 Layout。
- Validation Layer 下通过 Compute Shader 写 Storage Buffer，并在提交完成后验证结果。
- Clang 共享库、Visual Studio 共享库和 Clang 静态库三套矩阵通过。
- 公共头文件和示例不包含 Vulkan 头文件。
