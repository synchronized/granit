<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-20：Shader Asset 后端变体

## 状态

**已完成。** 后端 sidecar、裁剪、能力查询、确定性选择以及 WGSL/HLSL/GLSL portable 前端均已
形成闭环；不保留旧格式读取或迁移代码。

## 背景与目标

当前 `.granit-shader` 把 WGSL、SPIR-V 和反射数据整体嵌入一个文件。这样无法按目标后端裁剪
载荷，也容易把“源码语言”“运行后端”和“操作系统平台”混为一体。

本任务将 Shader Asset 调整为一个后端无关清单和若干后端载荷：

```text
example.granit-shader       # 契约、反射、缓存键、变体摘要与内容哈希
example.granit-shader.wgsl  # WebGPU portable 变体
example.granit-shader.spv   # Vulkan portable 变体
```

构建阶段可以产生全部已支持变体，打包阶段按目标后端保留所需文件。没有兼容变体时必须明确失败，
不能在运行时静默编译或退回含义不同的 Shader。

## 非目标

- 编译器只作为可选离线工具，不进入核心运行时依赖或公共 Renderer ABI。
- 不修改 `.grmat` 材质包和 Renderer 的 `granit_shader_desc`。
- 不按 Windows、Linux 等操作系统命名变体；选择依据是后端与能力档位。
- 不承诺 `.granit-shader` 为长期稳定的跨版本交换格式。

## 已确认决策

- `.granit-shader` 是唯一清单，后端代码存放在同名 `.wgsl`、`.spv` sidecar。
- 当前档位为 WebGPU portable WGSL 和 Vulkan portable SPIR-V。
- 清单保存反射、缓存键、载荷长度和 SHA-256；sidecar 缺失、损坏或摘要不符均视为未命中或
  不支持。
- 写入顺序为 sidecar 在前、清单在后，避免中断后让清单错误匹配混合版本载荷。
- 旧的内嵌式容器直接移除，不建立双格式读取和版本迁移分支。
- 后续 HLSL/GLSL 只是离线输入前端；最终仍生成上述后端变体，不成为新的运行时 API。
- 一个 Shader Asset 只描述一个入口点和阶段；同一源码的多个入口分别生成独立资产，使反射、缓存
  和失败诊断保持局部。
- portable 之外的能力版本使用独立候选资产，由调用方把其后端、档位、优先级和必需特性提交给
  `granit_renderer_select_shader_variant`。在真实可选能力启用前，不扩张清单为多载荷矩阵。

## 实施顺序

1. **S-20A 清单与 sidecar（已完成）**：拆分确定性资产，补齐摘要、损坏、缺失和缓存恢复测试。
2. **S-20B 变体选择契约（已完成）**：清单显式记录后端、代码格式、portable 档位和特性位，并以
   私有只读解析器按后端选择。多入口按独立资产生成，多能力版本按候选资产列表选择。
3. **S-20C 打包裁剪（已完成）**：工具可按 Vulkan、WebGPU 或全量目标导出；清单只声明实际
   保留的 sidecar，Vulkan 缓存恢复在缺少 Vulkan 变体时安全未命中。
能力发现与选择继续按以下顺序实施：

1. **S-20D 设备 Shader 能力（首版已完成）**：公共 Renderer 返回实际后端、portable 档位和已验证的
   可选 Shader 特性位；数值限制继续由 `granit_renderer_get_limits` 负责。
2. **S-20E 自动变体选择（首版已完成）**：Renderer 可按实际后端、portable 档位和特性位筛选
   调用方提供的变体元数据，并以确定性优先级返回索引；没有候选时返回 `unsupported`。数值要求
   和更细粒度结构化诊断留待对应特性实际启用时扩展。
3. **S-20F 工具目标能力（首版已完成）**：ShaderTools C/C++ API 与 CLI 已能列出、查询内置
   portable 目标；生成请求会校验必需特性，未知特性返回 `invalid_argument`，目标不支持则返回
   `unsupported`。目标能力来自发布契约，不能读取构建机 GPU 后假定部署设备相同。
4. **S-20G 可选源码前端（portable 首版已完成）**：ShaderTools 已可通过显式 DXC 生成 Vulkan 1.3
   SPIR-V，同时以独立的 portable 中间 SPIR-V 经锁定 Tint 生成 WGSL，并校验两条路径的反射契约
   一致；任一步不支持源代码能力都会明确失败且不保留不完整双产物。CLI 已可生成并按后端裁剪
   HLSL 资产；全后端资产已可按原始 HLSL、源码语言和完整编译上下文在启动编译器前恢复双产物。
   GLSL/glslang 前端也已复用相同的双产物、反射一致性、打包裁剪和原始源码缓存流程。前端只改变
   离线输入，不改变运行时变体选择和后端载荷格式。DXC、glslang 与 Tint 的统一发现、锁定版本和
   配置期能力探测已经收敛到可选 Shader Toolchain 契约。普通用户默认可使用通过能力探测的新工具版本，
   官方构建则严格锁定；CLI 会以实际工具二进制 SHA-256 自动隔离不同版本的资产缓存。

## 测试与验收

- 相同输入产生逐字节一致的清单和 sidecar。
- 任一 sidecar 缺失、截断或被篡改时不能命中缓存。
- 只保留 Vulkan 或 WebGPU 载荷时，选择结果与目标后端一致。
- 清单中的反射和最终后端代码经过现有严格 Binding 校验。
- Windows、Linux ShaderTools 与 Emscripten 资产消费路径通过各自验证。

## 风险与未决问题

- portable 是无可选特性要求的最低档位；`float16`、subgroup 等只有在具体后端完成启用和跨后端
  测试后才能进入 `supported_features`，不能根据 API 理论能力直接宣称支持。
- HLSL 的 Vulkan 产物保持 Vulkan 1.3；仅 Tint 转换桥使用 SPIR-V 1.3。提升桥接版本前必须先确认
  锁定 Tint 的 Reader 支持，不能让该限制反向降低 Vulkan sidecar 的目标版本。
- 跨多个文件的更新无法形成文件系统事务；清单最后提交和摘要校验负责把残留状态降级为安全失败。
- `.grmat` 是否改为引用 Shader Asset，仍由后续材质资产任务基于真实复用需求决定。
- 可下载工具链包及归档摘要属于 [S-21](S-21-shader-toolchain-package.md)，不阻塞本资产契约完成。
