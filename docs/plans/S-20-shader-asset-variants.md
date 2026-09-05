<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-20：Shader Asset 后端变体

## 状态

**实现中。** S-20A、S-20B 的首个 portable 档位和 S-20C 的按后端裁剪已经完成；不保留旧格式
读取或迁移代码。

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

- 本阶段不引入 HLSL、GLSL 前端或新的编译器依赖。
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

## 实施顺序

1. **S-20A 清单与 sidecar（已完成）**：拆分确定性资产，补齐摘要、损坏、缺失和缓存恢复测试。
2. **S-20B 变体选择契约（部分完成）**：已显式记录后端、代码格式、portable 档位和特性位，并以
   私有只读解析器按后端选择；多能力档位、入口点和阶段仍待扩展。
3. **S-20C 打包裁剪（已完成）**：工具可按 Vulkan、WebGPU 或全量目标导出；清单只声明实际
   保留的 sidecar，Vulkan 缓存恢复在缺少 Vulkan 变体时安全未命中。
能力发现与选择继续按以下顺序实施：

1. **S-20D 设备 Shader 能力（首版已完成）**：公共 Renderer 返回实际后端、portable 档位和已验证的
   可选 Shader 特性位；数值限制继续由 `granit_renderer_get_limits` 负责。
2. **S-20E 自动变体选择**：以 Renderer 能力筛选后端、档位、特性位和数值要求，按确定性优先级
   选择最合适变体；没有候选时返回 `unsupported` 和结构化诊断。
3. **S-20F 工具目标能力（查询已完成）**：ShaderTools C/C++ API 与 CLI 已能列出、查询内置
   portable 目标；生成请求的特性验证仍待接入。目标能力来自发布契约或调用方提供的设备快照，
   不能读取构建机 GPU 后假定部署设备相同。
4. **S-20G 可选源码前端**：先评估 HLSL，再评估 GLSL；前端只改变离线输入，不改变运行时变体
   选择和后端载荷格式。

## 测试与验收

- 相同输入产生逐字节一致的清单和 sidecar。
- 任一 sidecar 缺失、截断或被篡改时不能命中缓存。
- 只保留 Vulkan 或 WebGPU 载荷时，选择结果与目标后端一致。
- 清单中的反射和最终后端代码经过现有严格 Binding 校验。
- Windows、Linux ShaderTools 与 Emscripten 资产消费路径通过各自验证。

## 风险与未决问题

- 多入口、多能力档位的命名和优先级将在 S-20B 固化，S-20A 不提前扩张公共结构。
- portable 是无可选特性要求的最低档位；`float16`、subgroup 等只有在具体后端完成启用和跨后端
  测试后才能进入 `supported_features`，不能根据 API 理论能力直接宣称支持。
- 跨多个文件的更新无法形成文件系统事务；清单最后提交和摘要校验负责把残留状态降级为安全失败。
- `.grmat` 是否改为引用 Shader Asset，等待 S-20B 的真实复用结果，不在本任务前半段耦合修改。
