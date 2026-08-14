<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-02：材质参数、Shader 变体与离线构建

## 状态

- 路线图任务：H-02
- 优先级：P2
- 状态：已完成内部原型，尚未作为独立公共模块安装导出
- 前置依赖：D-01～D-08、H-01
- 后续依赖：H-03～H-07
- 格式计划：[H-02E3 持久化材质包](H-02-material-package-format.md)
- 历史记录：[H-02 实施记录](../records/H-02-material-system-implementation.md)

## 目标与边界

H-02 在核心 Renderer 之上提供可选材质模块，把命名参数、Texture/Sampler 绑定、固定渲染状态和
预编译 Shader 变体组织为可复用对象。普通使用者不需要逐项创建 Pipeline Layout、Bind Group 或
Graphics Pipeline，但仍可绕过材质模块使用底层显式 API。

材质系统不负责 Scene、Entity、Mesh 资产、灯光、可见性、文件监视或源 Shader 编辑器，也不把
PBR 特有字段固化到通用材质接口。

## 分层模型

```text
离线材质描述与 Shader
          -> 版本化材质包
          -> Material Template（不可变定义与 Pipeline 缓存）
          -> Material Instance（参数值与资源绑定）
          -> Renderer
```

### Material Template

- 保存参数和资源槽的稳定 ID、类型、默认值与显式布局。
- 保存 Pass、合法 Shader 变体和固定 Pipeline 状态。
- 保存创建 Bind Group Layout、Pipeline Layout 与 Graphics Pipeline 所需信息。
- 保持不可变；热替换通过新对象和上层原子引用替换完成。

### Material Instance

- 引用一个 Template，并保存常量 shadow buffer、资源绑定和静态特性值。
- 数值更新合并 dirty 区间后批量上传，不为每个 setter 单独提交 GPU 工作。
- Texture/Sampler 变化事务式重建 Bind Group，数值变化不重建 Layout 或 Pipeline。
- 不拥有导入的 Texture View 或 Sampler；内部 Uniform Buffer 和 Bind Group 由实例拥有并安全退役。

### Material Package

- 运行库只消费预编译 SPIR-V 和 Granit 自有元数据，不链接运行时 Shader 编译器或反射库。
- 持久化格式使用定宽记录、版本、目标环境、内容哈希和长度校验，不序列化 C++ 对象。
- 格式、编解码、调试 JSON 和输入限制以
  [H-02E3 计划](H-02-material-package-format.md)为权威来源。

## 参数与绑定约定

通用参数覆盖定宽标量、float vector、4×4 matrix、Texture View 和 Sampler。运行时按稳定参数 ID
查询；类型、offset、size、数组长度和 stride 均由离线元数据明确给出。

内置高层模块采用以下频率约定：

| Group | 频率 | 典型内容 |
|---:|---|---|
| 0 | frame/view | Camera、时间和全局数据 |
| 1 | material | 材质常量、Texture 和 Sampler |
| 2 | object | 变换、蒙皮和对象索引 |
| 3 | pass/lighting | 阴影、IBL、光源和后处理输入 |

通用底层 Pipeline API 仍允许自定义 Layout。内置模板的所有变体必须保持材质级布局兼容。

## Shader 变体与 Pipeline

- 只有改变 Shader 代码、Layout 或固定状态的静态特性才能形成变体。
- 普通颜色、粗糙度和资源句柄保持运行时参数，避免组合爆炸。
- 离线工具显式生成合法组合并计算稳定 key；缺失变体返回错误，不静默选择近似项。
- 完整 Pipeline 键还包含目标格式、采样数、顶点布局和固定状态，不能只使用变体 key。
- 同一键的并发请求合并创建；异步调度由外部 Job System 负责，材质模块不创建线程池。

## 工具链与运行时边界

- 首版 Shader 源语言为 HLSL，使用外部 DXC 生成 Vulkan 1.3 SPIR-V。
- 可用时使用 `spirv-val` 校验，并以 SPIRV-Reflect 转换成 Granit 自有元数据。
- DXC、SPIRV-Reflect 和 SPIRV-Headers 只属于离线工具，不进入运行库 ABI。
- 编译器版本、参数和输入共同参与可复现内容身份；升级工具链必须重新执行固定 Shader 测试。

## 完成结果

H-02 已完成内部原型：

- 材质元数据、稳定参数 ID、布局校验和 CPU shadow buffer。
- Uniform Buffer、Texture/Sampler Bind Group 和 dirty 区间上传。
- DXC、SPIR-V 校验与反射工具链原型。
- 内存及持久化材质包、稳定变体查找和 Pipeline 缓存。
- 参数/GPU 实例迁移、热替换槽和错误材质回退。
- 端到端示例和参数更新、变体查找、迁移、缓存命中性能基线。

逐阶段过程见[H-02 实施记录](../records/H-02-material-system-implementation.md)。

## 非目标

- 不在运行时解析或编译 GLSL/HLSL。
- 不实现 Shader Graph、可编程材质节点或完整资产系统。
- 不自动推导 PBR 参数、渲染队列或透明排序。
- 首版不要求 Bindless、Descriptor Buffer、Ray Tracing 或 Mesh Shader。
- 不把材质、Scene 或资产文件类型加入核心 Renderer ABI。

## 验收标准

- 材质模块不暴露 Vulkan 或第三方反射类型，核心 Renderer 不反向依赖材质模块。
- 参数、资源、变体、Pipeline 和包错误均返回明确结果，不以静默回退掩盖错误。
- Template、Shader、Layout 和 Pipeline 保持不可变，在途 Recorder 可以安全持有旧版本。
- 包构建确定、可验证且能够导出可读调试内容。
- 高频参数更新和缓存命中具有可复现性能数据。
