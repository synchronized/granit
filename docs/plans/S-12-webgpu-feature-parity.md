<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-12：WebGPU 公共能力补齐

## 状态

- 实现状态：已确认，待开始
- 前置依赖：S-10
- 后续任务：S-13
- 优先级：P1

## 背景与目标

S-10 已打通 Emscripten WebGPU 的公共 Renderer、Canvas 和无顶点输入三角形闭环，但普通桌面
应用还不能通过公共 API 选择 WebGPU，浏览器路径也不足以渲染带顶点、索引、纹理和动态 Uniform
的模型。S-12 负责补齐 S-13 模型查看器所需的最小跨后端能力，而不是追求 Vulkan 与 WebGPU 的
全部功能完全对等。

目标包括：

- 提供后端无关的 Renderer 选择与实际后端查询，不暴露 Vulkan、Dawn 或 WebGPU 原生类型。
- 让 Vulkan、桌面 Dawn WebGPU 与 Emscripten WebGPU 共用 Buffer、Texture、Sampler、Bind Group、
  Graphics Pipeline、动态 Uniform、Indexed Draw 和上传路径。
- 以同一组公共 API 测试验证资源所有权、错误语义、能力降级和绘制结果。

## 非目标

- 不在本任务实现 glTF、场景编辑器、资产缓存或模型格式。
- 不要求 WebGPU 支持 Vulkan 的全部格式、同步、查询、Bindless 或高级渲染能力。
- 不增加公开的 Vulkan/WebGPU 原生互操作接口。
- 不把 Dawn、Emscripten 或 WebGPU 头文件传播给 Granit 使用者。

## 已确认决策

- 在可扩展 Renderer 创建描述尾部增加后端偏好；至少表达 `auto`、`vulkan` 和 `webgpu`。
- 创建成功后允许查询实际后端，`auto` 的平台默认值和回退顺序必须明确且可测试。
- 不复制公共 Registry；具体能力继续通过现有私有 HAL 和后端工厂实现。
- 不用“静默不绘制”表达能力不足；不支持的组合返回稳定结果码，并由能力查询提前暴露限制。
- 高频绘制继续使用命令记录和批量提交，不为每个顶点、纹理或 Uniform 更新增加独立动态库调用。

## 实施顺序

1. **S-12A 后端选择**：设计 C ABI 与 C++ 包装，定义默认值、显式选择、不可用后端和回退语义。
2. **S-12B 几何资源**：接通 WebGPU Vertex/Index Buffer、顶点布局、索引格式和 Indexed Draw。
3. **S-12C 材质资源**：接通 Texture、Texture View、Sampler、Bind Group 与 PBR 所需基础格式。
4. **S-12D 每帧数据**：将动态 Uniform Offset、对齐限制和逐帧上传路径映射到两个后端。
5. **S-12E 跨后端 Fixture**：同一带纹理索引 Mesh 在 Vulkan、桌面 WebGPU 和浏览器 WebGPU 绘制。
6. **S-12F 验收**：验证公共头、共享/静态安装 Consumer、错误路径、截图结果和平台矩阵。

## 测试与验收

- C11 与 C++20 Consumer 能创建指定后端并查询实际后端。
- 后端不可用、能力不足、资源跨 Renderer 混用和动态偏移错误均有确定结果码。
- 同一确定性 Fixture 在 Vulkan、桌面 Dawn WebGPU 和 Emscripten WebGPU 产生容差内一致的结果。
- Windows、Linux 和 Emscripten 手动 Actions 通过；安装包不泄漏任何后端实现依赖。
- 完成后，S-13 不需要使用后端条件编译即可创建模型查看器所需 GPU 资源。

## 风险与未决问题

- 公共后端选择属于新增 ABI，必须先确定 `struct_size` 兼容规则和默认行为。
- WebGPU 格式、映射和提交完成语义与 Vulkan 不完全对等，需要能力查询与明确降级。
- 浏览器下载、解码和文件系统不属于 Renderer；S-13 应保持资源来源与渲染后端分离。
