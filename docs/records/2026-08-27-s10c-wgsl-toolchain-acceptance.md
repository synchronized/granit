<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10C WGSL Shader 工具链验收

## 结论

S-10C 已完成。WGSL 到双后端资产的编译、反射、确定性缓存、真实 Tint 诊断和离屏像素闭环均已在
Windows 与 Linux 通过。当前不向核心 Renderer 增加公共 Shader 资产创建 API。

## 验收范围

- 同一 WGSL 生成包含 WGSL、Vulkan 1.3 SPIR-V 和稳定反射 JSON 的 `.granit-shader` 资产。
- WebGPU 消费资产中的 WGSL，Vulkan 消费同一资产中的 SPIR-V。
- 验证首次编译、缓存恢复、资产大小、无效 WGSL 诊断及失败时不留下输出文件。
- 在 Windows D3D12 和 Linux Vulkan/Lavapipe 上运行真实 Dawn smoke test；Linux 同时运行普通
  Vulkan Shader 资产离屏像素回归。

## 跨平台结果

| 平台 | 首次编译 | 缓存恢复 | Vertex 资产 | Fragment 资产 | 运行结果 |
| --- | ---: | ---: | ---: | ---: | --- |
| Windows D3D12 | 101 ms | 6 ms | 1739 bytes | 1296 bytes | WebGPU 通过 |
| Linux Vulkan/Lavapipe | 6 ms | 3 ms | 1739 bytes | 1296 bytes | WebGPU、Vulkan 通过 |

上述时间是 GitHub 托管 Runner 上单次验收样本，只用于确认缓存路径生效，不作为稳定性能承诺。
Linux WebGPU Queue Submit 为 2852 us，总耗时 1044 ms；Windows Queue Submit 为 17753 us，
总耗时 211 ms。

## 诊断结果

无效 Fixture 在两个平台均由真实 Tint 报告
`s10c_invalid.wgsl:7:10`、入口点 `vs_main`、阶段 `vertex` 和退出码 1。工作流同时确认编译失败后
不存在目标 SPIR-V 文件，结构化诊断没有吞掉 Tint 原始错误。

## 公共 API 决策

S-10C 不增加核心 Renderer 的 `granit_shader_create_asset` 一类公共入口，原因如下：

- 当前稳定 Vulkan Shader 公共接口仍以 SPIR-V 为输入，实验性 WebGPU 插件负责 WGSL 路径。
- `.granit-shader` 仍是可重建的私有构建产物，0.4.0 不承诺其长期文件格式稳定性。
- 立即增加资产创建入口会与现有 SPIR-V 创建路径形成含义重叠的公共 API。

编辑器和资产构建器继续使用独立 ShaderTools SDK；缓存恢复和资产编解码保持在该组件及内部实现。
待 WebGPU 进入公共 Renderer 抽象，或 S-10D/S-10E 明确运行时资源加载契约后，再审议统一入口。

## 验证来源

- [Dawn 双平台验收](https://github.com/synchronized/granit/actions/runs/33057090314)
- [S-10C WGSL Shader 工具链计划](../plans/S-10C-wgsl-toolchain.md)
