<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-37L：HLSL-first Shader 作者入口

## 状态

**实现中。** Granit 的离线 Shader 作者语言收敛为 HLSL；WGSL 与 SPIR-V 继续作为 ShaderTools
生成的后端载荷。S-37L1 已完成源清单、逻辑名称与确定性索引模型；S-37L2 已完成高层 Builder、
CLI 与 CMake 入口；S-37L3 已完成 Material 逻辑引用与构建期索引解析；S-37L4 已完成 PBR、
Unlit、Canvas、Shadow、Tone Mapping、Debug 和 Smoke 资产迁移。S-37L5 已将命令行和公共
Compiler 收敛为 HLSL，且已删除 `shader_source_language`、成对载荷导入接口及公开的 Shader
Object 写入与缓存接口，低层 Library 链接入口也已内化；产品 Shader 目录中的派生载荷已删除，
无工具链测试所需的快照集中到 `tests/fixtures/generated`。S-37L6 已建立 ShaderTools 导出快照、
C/C++ 安装 Consumer 与构建期 Shader 产物安装门禁；私有 Object 命令也已移出标准 CLI。其余
跨平台验收在当前 0.21 特性分支连续实施。Windows 共享与静态完整测试、两种链接模式的安装
Consumer、Emscripten 构建及其宿主测试已经通过；Linux 与浏览器真实 WebGPU 验收仍待对应环境
执行。每个阶段形成独立本地提交。

## 背景与目标

S-37K 已经明确 Compiler、Compilation、Reflection、私有 Shader Object 与运行时 Shader Library
的边界，但作者入口仍同时接受 WGSL、HLSL 和成对的预生成 WGSL/SPIR-V。内建资产也混合维护
HLSL 源码和后端载荷，导致源码权威、CMake 入口和材质引用方式不统一。

本计划将标准数据流固定为：

```text
HLSL + Shader Library 源清单
              │
              ▼
       Shader Library Builder
         ├─ DXC → Vulkan SPIR-V
         ├─ DXC → portable SPIR-V → Tint → WebGPU WGSL
         ├─ 跨后端反射与绑定契约检查
         └─ 私有 .grshaderobj 缓存
              │
              ├─ .grshlib       运行时发布资产
              └─ .grshidx.json  构建期逻辑名称索引
                                      │
Material 源清单 ──────────────────────┘
              │
              ▼
          .grmat 中的 content_id
```

普通作者只维护 HLSL、`.grshlib.json` 和 `.grmat.json`。构建过程管理 WGSL、SPIR-V、
`.grshaderobj`、内容 ID 和缓存，不要求应用或材质作者识别 Renderer 后端。

## 非目标

- 不删除运行时 `granit_shader_code_format`、WGSL 载荷或 SPIR-V 载荷。
- 不改变 `.grshlib` v1、`.grmat` v5、运行时内容 ID 查找和 Shader Library 生命周期。
- 不在 Core、Renderer、应用或浏览器运行时启动 DXC、Tint 或文件系统构建流程。
- 不发明 HLSL 的替代语言，也不把材质状态、Pass 或 Pipeline 状态嵌入 HLSL。
- 不保留旧 WGSL 作者入口、成对载荷导入入口或 Material 源清单兼容解析。
- 不在本计划引入节点图、远程编译服务、增量资产数据库或新的 Renderer 后端。

## 已确认决策

### HLSL 是唯一离线作者语言

安装后的 ShaderTools Compiler 只接收 HLSL。`shader_source_language`、WGSL 编译分支和公开
`source_language` 字段已经删除。每个逻辑 Shader 只有一份 HLSL 源码，不允许用手写
WGSL 覆盖某个后端。

Shader 必须符合 `portable` 档位：显式声明资源 binding，使用两端都能转换和验证的语言能力，并
固定矩阵主序、资源布局和数值语义。任一目标后端生成失败或反射契约不一致时，整个 Library 构建
失败。

### 后端载荷格式继续属于运行时契约

HLSL-first 只约束离线作者输入。Vulkan 仍消费 SPIR-V，WebGPU 仍消费 WGSL；底层
`granit_shader_create()` 继续通过 `granit_shader_code_format` 接收单一后端载荷，正式资产继续通过
`granit_shader_create_from_library()` 按内容 ID 选择载荷。

### Shader Library 源清单是统一作者入口

新增 `.grshlib.json` 源清单，至少声明：

- Library 逻辑名称、格式版本、目标后端和能力档位；
- 每个 Shader 的稳定逻辑名称、HLSL 路径、阶段和入口点；
- 显式命名的变体及其有序 Define 集合；
- 可选的预期绑定契约。

逻辑名称在单个 Library 内唯一，不依赖文件路径。变体使用
`shader-name/variant-name` 形成完整名称。逻辑名称不进入 Shader 内容 ID；源码、入口、阶段、Define、
工具身份、目标档位和生成载荷决定缓存与内容身份。

首版清单示意：

```json
{
  "format_version": 1,
  "name": "pbr_standard",
  "target_profile": "portable",
  "target_backends": ["vulkan", "webgpu"],
  "shaders": [
    {
      "name": "standard.vertex",
      "source": "pbr_untextured.hlsl",
      "stage": "vertex",
      "entry_point": "vertex_main"
    },
    {
      "name": "standard.fragment",
      "source": "pbr_untextured.hlsl",
      "stage": "fragment",
      "entry_point": "fragment_main",
      "variants": [
        {"name": "untextured", "defines": {"GRANIT_PBR_TEXTURE_MASK": "0"}},
        {"name": "textured", "defines": {"GRANIT_PBR_TEXTURE_MASK": "31"}}
      ]
    }
  ]
}
```

### 名称索引只参与构建

Builder 同时生成确定性的 `.grshidx.json`，保存 Library 名称、Library 摘要，以及逻辑 Shader 名称
到 `content_id`、stage 和 entry point 的映射。索引不进入 Runtime component，也不增加运行时字符串
查找；最终 `.grmat` 仍只保存稳定内容 ID 和创建所需元数据。

Material 源格式改为引用逻辑名称：

```json
{"library": "pbr_standard", "shader": "standard.fragment", "variant": "textured"}
```

Material Tool 必须显式接收一个或多个 Shader 索引。重复 Library 名称、未知 Shader、未知变体、索引
摘要冲突和阶段组合错误均使构建失败。旧的手写 `content_id/stage/entry_point` 源格式不保留。

### 高层 Builder 编排现有工具能力

`shader_library_builder` 成为面向使用者的源码构建入口，负责解析清单、并行无关编译、对象缓存、
Library 链接和索引写入。现有单 Shader Compiler 与 Reflection API 继续支持编辑器和诊断工具；从
成对载荷构建 Object、直接链接 Object 的入口降为 ShaderTools 私有实现。

CLI 的标准入口收敛为：

```text
granit_shader_tool build-library --manifest <file.grshlib.json>
                                  --output <file.grshlib>
                                  --index <file.grshidx.json>
```

CMake 对项目公开一个对应函数。CMake 只声明输入、输出和依赖，清单语义、缓存身份及格式编码全部
由 ShaderTools 实现。

## 实施顺序

1. **S-37L1 源清单与逻辑名称模型（已完成）**：实现 `.grshlib.json` 数据模型、严格解析、路径
   约束、名称与 Define 规范化、确定性索引编码及失败测试；暂不改变现有构建入口。
2. **S-37L2 高层 Library Builder（已完成）**：增加 C11/C++20 一次性构建接口和
   `build-library` CLI，编排
   HLSL 编译、跨后端反射检查、私有 Object 缓存、Library 链接和索引生成。
3. **S-37L3 Material 逻辑引用（已完成）**：让 Material Tool 接收 Shader 索引；将 `.grmat.json`
   作者格式升级到版本 6，并在打包时解析逻辑名称；二进制 `.grmat` 继续使用 v5。
4. **S-37L4 内建资产迁移（已完成）**：为 PBR、Unlit、Canvas、Shadow、Tone Mapping、Debug 和
   Smoke 建立 HLSL Library 清单；通过 Define 生成纹理、阴影、IBL、灯光和颜色编码变体，重建
   最终资产。
5. **S-37L5 删除多源作者入口（已完成）**：删除 WGSL 编译公共入口、`shader_source_language`、成对
   WGSL/SPIR-V Object 导入、旧 CLI/CMake 作者入口，以及源码树中的手写 WGSL 和预生成 SPIR-V。
6. **S-37L6 发布边界与验收**：更新 Reference、Guide、迁移文档、ABI 快照、安装清单和 Toolchain
   包；验证 Runtime 安装只包含最终 Library/Material，ShaderTools 安装只公开 HLSL-first 入口。

阶段在当前特性分支连续开发，不拆分 Pull Request；每个阶段完成相关测试后创建一个聚焦的本地
提交。

## 测试与验收

- 清单覆盖未知字段策略、重复名称、非法路径、无效阶段/入口、空变体、重复 Define 和无效目标。
- 相同清单、源码、工具身份和选项逐字节生成相同 `.grshlib` 与 `.grshidx.json`。
- HLSL 的 Vulkan SPIR-V、portable 中间 SPIR-V 和最终 WGSL 具有一致入口、阶段与绑定契约。
- PBR 各纹理掩码及 Shadow/IBL/Lights 组合由同一 HLSL 源码生成，并覆盖现有渲染路径。
- Material 源文件只使用逻辑名称；最终 `.grmat` 不携带索引路径、Library 名称或作者格式字符串。
- 产品 Shader 目录不再包含作为作者输入的 `.wgsl`、提交的 `.spv` 或 `.grshaderobj`；测试夹具可
  保留不参与安装和发布的后端载荷快照。
- 公共 ShaderTools 头、导出符号和 C/C++ Consumer 不再出现 `shader_source_language` 或 WGSL 编译
  入口；Core 运行时仍完整支持生成的 WGSL 载荷。
- Windows 共享/静态、Linux GCC/Clang、Emscripten、浏览器 WebGPU、构建树及安装 Consumer 通过。

## 风险与未决问题

- HLSL 到 WGSL 的链路依赖 DXC 与 Tint 对同一可移植子集的支持；迁移必须以最终 WGSL 验证和真实
  WebGPU Smoke 为准，不能只检查 DXC 成功。
- PBR 组合数量可能快速增长。首版只编码当前实际使用的显式变体，不自动计算任意宏的笛卡尔积。
- 删除已提交 SPIR-V 前必须确认所有 HLSL 变体能够重建相同绑定和可接受的视觉结果；不要求二进制
  字节相同。
- `.grshidx.json` 是构建期派生文件，不承诺运行时或跨大版本兼容；其确定性和严格校验仍需测试。
- 工具链目前逐项启动 DXC/Tint。完成正确性迁移后再依据测量决定并行度和进程复用，不在 L1 提前
  引入调度抽象。
