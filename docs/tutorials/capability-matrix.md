<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Tutorial 能力矩阵

本文是 S-82 的基线，记录 Granit Tutorial 对常见图形特性的验证入口。它只描述当前仓库已有证据；
新教程完成后再补充验收结果，不能把规划中的探针写成已实现能力。

## 当前覆盖

| 能力方向 | 当前入口 | 基线结论 |
|---|---|---|
| Window、Renderer、Texture、Mesh、深度、Canvas | `01_cube` | 已有低层绘制路径 |
| PBR、Mask/Blend、阴影、HDR、Tone Mapping、IBL | `02_pbr_assets`、Model Viewer | 已有参考管线，缺少独立最小透明探针 |
| Per-instance Vertex Buffer 与动态 Uniform | `03_instancing` | 已有单次实例绘制 |
| 全屏绘制与程序化 Fragment | `04_raymarch`、`05_metaballs` | 已有跨后端 Shader 验证 |
| Compute、Atomic、Transient Buffer、Indirect Draw | `06_marching_cubes` | 已有 GPU 生成几何路径 |
| MRT、中间纹理、多 Pass、Resize | `07_deferred` | 已有 Deferred 路径 |
| 外部 Window、SDL3、ImGui、Canvas | `08_sdl_imgui` | 已有宿主集成边界 |
| 动态几何、透明粒子、容量变化 | `09_particles` | S-82B 待实现 |
| Opaque、Mask、Blend、对象级透明排序 | `10_transparency` | S-82C 待实现 |
| 格式、Usage、Sampler、Mip、Indirect 能力查询 | `11_capabilities` | S-82D 待实现 |

## 当前缺口候选

以下项目只作为探针结果的候选，不代表已经决定实现：

- Stencil State 尚无独立教程和完整公共 Pipeline 契约；
- OIT 超出当前对象级透明排序语义；
- SVT/虚拟纹理属于 Gneiss 资源驻留和调度方向，不由教程直接下沉到 Granit；
- GPU-driven 多批次需要独立的跨后端批量提交语义，不能由现有单次 Indirect Draw 直接推定支持。

## 缺口分类

教程验收后按以下顺序归类：

1. 教程本地数学、数据组织或 UI：留在 `examples/tutorials`；
2. 多个教程重复且语义稳定的 C++ 便利逻辑：记录证据后单独评审；
3. C API/C++ 包装摩擦：补齐轻量包装和负向测试；
4. 跨后端公共 GPU 能力缺失：建立独立版本计划；
5. 单后端或高成本能力：明确能力查询、降级或不支持边界。

## 验收基线

每个新教程必须使用 Vulkan 与 WebGPU 的同一份 C++ 主体，提供固定参数的 Smoke 模式，并覆盖
多帧推进、Resize、失败清理和浏览器 Validation 消息。教程不能包含 Vulkan/WebGPU 类型，也不能
包含 `src/` 私有头。
