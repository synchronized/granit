<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 资产目录

本目录只保存 Granit 自有的作者输入和需要提交到仓库的确定性生成快照。普通构建产生的临时文件、
Shader Object 与嵌入用 `.inc` 写入 `build/generated`，不得写回源码目录。

## 目录职责

```text
assets/
├─ sources/                       # AssetTools 作者输入
│  ├─ shaders/                    # HLSL 与 .grshlib.json
│  └─ materials/                  # .grmat.json
└─ generated/                     # 锁定工具链生成并提交的权威快照
   ├─ installed/                  # SDK 对外运行时资产及生成伴随信息
   │  ├─ libraries/
   │  ├─ materials/
   │  └─ environments/
   └─ embedded/                   # 编译进 Granit 模块的私有资产
```

`sources` 中的文件由开发者维护，是资产构建的输入。`generated` 中的文件必须能够由锁定工具链或
对应 AssetTools 命令重新生成，并在构建中与新输出逐字节比较。

`generated/installed` 的公开文件安装到 `share/granit/assets`，源码树中的额外索引只用于生成和
验证，不随 SDK 安装。`generated/embedded` 的内容只用于构建 Granit 自身，不安装或形成公共契约。

测试输入和测试专用 SPIR-V/WGSL 快照位于 `tests/fixtures`，不属于本目录，也不进入安装包。

## 修改规则

- 修改 HLSL、Library Manifest 或 Material Source 时，应使用锁定 Shader Toolchain 重建对应快照。
- 更新 `.grshlib` 时必须同步更新 `.grshidx.json`、引用它的 `.grmat` 和生成的内容 ID。
- 更新 Environment 时必须同步更新 `.grenv`、Manifest 中的大小和摘要。
- 安装规则显式列出公开文件，不允许递归安装整个 `generated/installed`。
- 不提交 AssetTools 对象缓存或 Build Tree 中间文件。
