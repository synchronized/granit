<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# H-02E3：持久化材质包格式

## 元数据

- 设计状态：已确认
- 实现状态：进行中（H-02E3A 已完成）
- 路线图任务：H-02E3
- 优先级：P2
- 前置依赖：H-02E1、H-02E2
- 后续依赖：H-02F、H-02G

## 三种表示

材质构建流程区分三种用途不同的表示：

| 表示 | 用途 | 是否由 Renderer 读取 |
| --- | --- | --- |
| `.grmat.json` | 人工编写或外部格式转换得到的源描述 | 否 |
| `.grmat` | 版本化二进制运行时包 | 是 |
| `.grmat.debug.json` | 从最终二进制包导出的只读诊断视图 | 否 |

源描述和调试视图不是同一种格式。调试 JSON 不允许重新作为构建输入，避免把诊断字段误当成稳定
资产接口。运行库不回退读取 JSON，也不在加载时调用 Shader 编译器。

## 文件组织

`.grmat` 首版采用固定文件头、固定尺寸区段目录和独立数据区段：

```text
File Header
Section Directory
String Table
Material Metadata
Feature Definitions
Pass Definitions
Variant Records
Shader Records
SPIR-V Data
Build Metadata（可选）
Dependency Metadata（可选）
```

所有多字节整数使用小端编码。文件结构不得直接序列化 C/C++ 对象、指针、`size_t`、枚举内存或
STL 容器。记录之间只使用定宽索引和相对文件起点的 64 位 offset。

### 文件头

文件头至少包含：

- 8 字节 magic：`GRMAT` 加零填充。
- 32 位格式主版本与次版本。
- 32 位小端标记 `0x01020304`。
- 文件头尺寸、区段记录尺寸和区段数量。
- 区段目录 offset、文件总长度。
- 目标环境和绑定模型。
- 必需 Renderer feature flags。
- 规范化内容的 SHA-256。

早期开发阶段读取器只接受完全匹配的主版本。次版本只允许增加可选区段或向已定义记录尾部追加
具有默认语义的字段；尚未发布稳定版本前不提供自动迁移承诺。

### 区段目录

每个区段记录包含：

- 32 位区段类型。
- `required`、`compressed` 等 32 位 flags。
- 64 位 offset、存储长度和解压后长度。
- 32 位对齐要求。
- 区段内容校验值或保留字段。

读取器可以跳过未知可选区段，遇到未知必需区段必须拒绝。首版不允许压缩；`compressed` 只保留
语义，任何置位的压缩区段均返回不支持。

## 必需区段

### String Table

保存 UTF-8 字节。其他记录通过 `offset + length` 引用，不依赖零结尾。相同字符串由构建器去重，
但读取器不能依赖去重才能正确加载。

### Material Metadata

保存常量块尺寸以及参数的稳定 ID、名称引用、类型、offset、数组数量、stride、resource binding
和默认值范围。包内数据转换成现有 `metadata_desc` 后，仍必须经过 `material_metadata::build` 的完整
语义校验。

### Feature、Pass 与 Variant

- Feature 保存稳定 ID、名称、允许值和默认值。
- Pass 保存稳定 ID、名称、固定 Pipeline 状态和其 Variant 范围。
- Variant 保存规范化 feature 列表、变体键、顶点输入要求及 Shader 记录范围。
- 读取器必须重新计算变体键，不能盲目信任文件中的缓存值。

### Shader 与 SPIR-V

Shader 记录保存阶段、入口名称引用、SPIR-V offset、长度和内容哈希。SPIR-V 数据保持 4 字节对齐，
长度必须是 4 的倍数，并在创建 Shader 前执行与公开 Shader API 一致的基础校验。

## 可选构建信息

开发包可以保存 DXC、反射工具、目标环境、规范化编译参数和输入依赖哈希。发布构建允许移除源路径
和依赖列表，但不能移除影响缓存身份的编译器、目标环境和参数摘要。

绝对路径、构建时间、主机名、随机 ID 和用户目录不得参与规范化内容哈希，也默认不写入包，以保证
相同输入产生相同输出并避免泄漏开发机信息。

## 安全与资源上限

材质包始终按不可信输入处理。读取器在分配内存或创建 GPU 对象前完成整包结构校验：

- 所有加法、乘法、对齐和 `offset + size` 检查整数溢出。
- 文件头、目录和区段必须位于文件范围内，区段不得相互重叠。
- offset 必须满足区段声明及记录类型要求的对齐。
- count 必须同时受文件剩余长度和显式实现上限约束。
- 字符串必须是合法 UTF-8，引用不得越过 String Table。
- 默认值、参数布局、feature、variant 和 Shader 阶段必须通过语义校验。
- SHA-256 不匹配、尾随未声明数据或重复必需区段均视为损坏。

首版把文件完整读入内存并验证，不实现 mmap、流式解析、异步 IO 或原地访问磁盘记录。

## 调试 JSON

构建时可显式生成伴随文件：

```powershell
granit_material_tool build standard.grmat.json `
  --output standard.grmat `
  --emit-debug-json standard.grmat.debug.json
```

省略调试路径时使用 `<output>.debug.json`。默认不生成调试 JSON。

现有二进制包可以独立检查：

```powershell
granit_material_tool inspect standard.grmat --json
granit_material_tool inspect standard.grmat --json --output standard.grmat.debug.json
```

`inspect` 必须先运行与 Renderer 加载相同的结构和语义校验，再输出最终包中的真实内容。失败时返回
非零退出码，诊断写入标准错误；指定输出文件时采用临时文件加原子替换，不能留下看似有效的半文件。

调试 JSON 使用固定字段顺序和缩进，参数按 ID、Pass 按 ID、Variant 按键、Shader 按阶段排序。
ID 和哈希输出为固定宽度小写十六进制字符串。默认只输出 SPIR-V 的 offset、长度和 SHA-256，不把
二进制转换为 Base64。输出不得包含时间戳、绝对路径或其他不可复现字段。

首版命令行只实现：

- `build --output <path> [--emit-debug-json [path]]`
- `inspect <path> --json [--output <path>]`

`--dump-spirv`、`--disassemble-spirv`、压缩和依赖路径导出留到基本格式及损坏输入测试稳定后评估。

## 实施顺序

1. **H-02E3A（已完成）**：已定义定宽文件头、区段目录、类型编号、对齐和显式实现上限，并实现
   结构解析与截断、字节序、重叠及未知区段测试。
2. **H-02E3B**：实现内存编码器、解码器、SHA-256 和完整结构校验。
3. **H-02E3C**：编码/解码现有 `material_package`，覆盖所有参数、Pass、Variant 与 Shader 数据。
4. **H-02E3D**：实现 `granit_material_tool build/inspect` 和稳定调试 JSON。
5. **H-02E3E**：增加截断、溢出、重叠、未知区段、哈希错误和随机字节损坏测试。

## 验收标准

- 相同规范化输入在不同构建目录生成逐字节相同的 `.grmat` 和调试 JSON。
- 解码后重新编码得到相同规范化二进制。
- 读取器在任何内存分配和 GPU 创建前拒绝结构损坏。
- 未知可选区段可跳过，未知必需区段和不支持的 feature 明确失败。
- Renderer 只依赖二进制解码结果，不依赖 JSON、DXC、反射库或源文件。
- 调试 JSON 来自最终二进制包，能够稳定参与 Git diff 和 CI 产物检查。
