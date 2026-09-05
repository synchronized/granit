<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-05 S-23A Shader 资产契约审计

## 结论

现有 `.grshader` schema 3 可以可靠保存后端变体摘要，但尚不能直接作为运行时 Shader Asset
契约：清单缺少 Shader 阶段和入口点，`cache_key` 又混合了编译工具身份，不能代替稳定资产内容
ID。0.8.0 需要先升级格式，再增加 Core 运行时入口和 Material 引用。

## 当前职责

| 位置 | 当前职责 | 0.8.0 处理 |
|---|---|---|
| `tools/shader_asset.*` | 清单编码、解码、SHA-256、变体选择、sidecar 文件原子写入 | 拆出无文件 I/O 的私有格式层 |
| `tools/shader_tools_api.cpp` | 编译、读取/写入路径、构建与解包资产 | 保留工具和文件职责，复用私有格式层 |
| `granit_shader_create` | 接受调用方已选择的 SPIR-V/WGSL | 保留底层入口，并增加资产选择入口 |
| `material_package_archive.*` | 在 `.grmat` 内再次保存 SPIR-V/WGSL | 改为保存 Shader Asset ID 与使用约束 |
| `material_api.cpp` | 解码 `.grmat` 并直接创建 Shader/Pipeline | 通过调用方 resolver 获取依赖字节 |
| Model Viewer/Smoke | 嵌入完整 `.grmat`，间接重复嵌入 Shader | 迁移为材质包和 Shader sidecar 依赖集 |

## 格式缺口

- schema 3 的变体记录包含 backend、code format、portable profile、required features、byte size 和
  payload digest，但清单本身没有 Shader stage 与 entry point。
- `cache_key` 包含源码、前端、入口、阶段、Tint 修订、目标环境、编译选项和能力，可用于构建缓存
  失效；工具升级后同一运行时内容可能产生不同缓存键，因此不能作为资产引用身份。
- schema 3 固定最多两个变体，没有区段目录；继续向固定头部追加字段会削弱未知可选字段和未来
  profile 扩展能力。
- `.grmat` 格式 3 的 Shader Records、SPIR-V Data 和 WGSL Data 与 `.grshader` sidecar 表达同一
  执行代码，形成两套摘要、校验、部署和迁移路径。

## 0.8.0 处理原则

- 新 `.grshader` 格式显式保存 stage、entry point、构建缓存键和规范化内容 ID；内容 ID 不包含路径、
  时间、主机或可变文件位置。
- 只读解析、边界校验、摘要计算和变体选择进入 Core 与 ShaderTools 共用的私有实现；原子文件写入、
  编译器调用和缓存目录只留在 ShaderTools。
- 公共运行时 API 接受清单及调用方提供的 sidecar 字节，不接受路径；Renderer 根据实际能力选择
  唯一变体。
- 新 `.grmat` 只记录 Shader Asset 内容 ID、阶段、入口和选择要求。旧格式尚未稳定，使用离线工具
  重新构建，不在运行时同时维护两套读取器。
- Material resolver 首版是创建期间同步借用回调；Granit 不保存回调返回指针，也不定义上游资产
  存储或线程模型。

## 验证基线

S-23A 开始时，Core 的公开 Shader 描述可同时携带 SPIR-V 与 WGSL；ShaderTools 已能生成按后端
裁剪的 `.grshader` 和 `.wgsl`/`.spv` sidecar；Material 包格式 3 仍要求内嵌两种载荷。后续阶段以
现有 Shader Asset、Material Archive、GPU Material、安装 Consumer 和跨后端 Model Viewer 测试
作为迁移回归基线。
