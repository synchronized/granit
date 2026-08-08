<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# R-06：Sampler 生命周期与能力限制

## 元数据

- 设计状态：已确认
- 实现状态：已完成
- 路线图任务：R-06
- 优先级：P0
- 前置依赖：R-02
- 后续依赖：D-02、D-03、R-08

## 已确认语义

- Sampler 是 Renderer 拥有的独立 64 位句柄，与 Texture/View 解耦。
- 首期支持 nearest/linear、nearest/linear mip、repeat、mirrored repeat 和 clamp to edge。
- 支持全部 Vulkan core 对比操作，不公开 Vulkan 数值。
- 各向异性仅在设备支持时启用，数值必须处于 `[1, maxSamplerAnisotropy]`。
- LOD bias 必须是有限值且不超过设备 `maxSamplerLodBias`。
- 超过设备能力返回不支持，不静默裁剪。
- 每次创建返回独立公开句柄，首期不缓存相同描述。
- Renderer 销毁级联清理 Sampler；异步使用后的延迟销毁由 R-08 负责。

## 实现结果

物理设备选择记录 sampler anisotropy feature，逻辑设备仅在支持时启用。Sampler 创建先执行后端
无关规范化验证，再检查选中设备 feature 与 limits，最后创建 `VkSampler`。

C/C++ 头文件、move-only RAII、错误 Renderer、重复销毁、compare sampler 和描述验证均有测试。
