<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10A Vulkan 耦合审计

## 结论

公共头文件没有包含 Vulkan SDK，也没有公开 `Vk*` 类型。多后端工作的主要切入点不是重写公共
C ABI，而是拆分内部 Registry 与 Vulkan 执行对象。高层 Pipeline、Lighting、Material 和
Render Graph 通过公共 Renderer API 工作，可以在首轮迁移中保持不变。

## 审计结果

| 区域 | 当前耦合 | 处理方向 |
|---|---|---|
| `renderer_state` | 设备、分配、资源、命令、提交和 Surface 方法使用 Vulkan 类型 | 收拢为 Vulkan 后端实现 |
| `renderer_registry` | 资源记录直接保存 `Vk*`、VMA 分配及 Vulkan 访问状态 | 改为拥有后端资源对象 |
| Shader | 公共创建描述只接受 SPIR-V | S-10C 另行设计输入格式，不阻塞内部迁移 |
| Surface/Swapchain | 公共平台句柄中立，内部创建与帧同步为 Vulkan 模型 | 将平台描述传给后端，由后端管理交换链 |
| 能力与限制 | Registry 直接读取 Vulkan device limits | 建立不可变的归一化内部能力快照 |
| 测试 | 公共 GPU 测试默认把不可用环境称为 Vulkan；后端测试直接编译 Vulkan 源文件 | 分离通用契约测试与 Vulkan 专属测试 |
| 构建 | `granit` 目标直接列出并配置全部 Vulkan 源与宏 | 后续拆为内部 Vulkan 对象库或静态实现目标 |

静态搜索显示耦合高度集中：`renderer_state.h/.cpp` 与 `renderer_registry.h/.cpp` 包含大量 Vulkan
类型或符号，而除 Swapchain API 外的公共 API 转发层和高层模块没有同等级的原生后端依赖。

## 建议迁移切片

1. 定义后端无关的内部能力、资源基类和粗粒度设备契约，并用测试替身验证生命周期。
2. 将 Vulkan 资源记录移入 Vulkan 实现对象，Registry 只保存内部资源接口。
3. 依次迁移 Buffer/Texture/Sampler、Shader/Pipeline、Recorder/Queue、Surface/Swapchain。
4. Vulkan 回归全部通过后，再比较 Dawn 与 wgpu-native，并实现桌面离屏 WebGPU MVP。

每个切片都应保持公共 ABI 和 Vulkan 行为不变，并测量命令记录与提交路径，防止抽象层引入逐命令
动态分派或额外锁竞争。
