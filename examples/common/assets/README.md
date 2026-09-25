<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Example Asset System 契约

本目录提供仓库示例共用的只读资产入口。它用于让 Desktop 和 Web 示例共享业务代码，属于
`examples/common` 私有实现，不安装、不导出，也不是 Granit 公共 VFS。

## 逻辑身份

业务代码以 `asset_key{mount, path}` 标识资产：

- `asset_mount` 决定根位置和读取来源；零值无效。
- `path` 是 Mount 内使用 `/` 分隔的相对逻辑路径。
- 路径会移除 `.` 并规范化分隔符；绝对路径、URL、空路径、NUL 和 `..` 跳转会被拒绝。
- `asset_request::location()` 只用于诊断，不作为可持久化身份。

随程序发布的资产使用 `asset_system::bundled()`。同一个逻辑 Key 在 Desktop 从可执行文件旁的
`assets` 目录读取，在 Web 从预加载的 `/assets` 读取；成功或失败请求都以
`asset:///<logical-path>` 报告诊断位置。示例业务代码不拼接平台文件路径。

外部模型或环境使用 `mount()` 挂载目录或 URL 根。入口是单个文件或 URL 时，
`mount_location()` 会把它拆成 Mount 和规范化逻辑路径。Desktop Source 在后台读取文件并由
`poll()` 发布完成结果；Web Source 使用 Emscripten Fetch，回调直接推进同一种 `asset_request`。

## 复合资产

glTF 内的 Buffer 和图片 URI 相对于主文档的逻辑目录解析，并始终留在同一个 Mount。路径规则由
`resolve_resource_path()` 负责，调用方只传主文档 Key，不重复添加目录前缀。

Shader Library 由构建过程生成并按目标选择嵌入或安装位置，它具有自身格式和加载接口，不作为普通
模型、纹理 Blob 交给 Example Asset System 管理。

## 部署

示例目标通过 `granit_example_target_assets(TARGET ... ROOT ... FILES ...)` 声明随程序发布的文件。
`FILES` 必须是 `ROOT` 下的规范相对路径：

- Desktop 构建后复制到目标可执行文件旁的 `assets/<logical-path>`。
- Emscripten 以同一个逻辑路径预加载到 `/assets/<logical-path>`。

教程提交的资产在各自 `examples/assets/.../README.md` 中记录来源、许可证、固定上游版本和
SHA-256。完整 Model Viewer 资产使用 manifest 下载与校验，不把大型外部模型提交到仓库。

只有出现至少两个仓库外使用者，并且 Mount、异步完成、取消、缓存和线程语义稳定后，才另行评估
公共资产 API；当前代码不能被公共头文件或安装目标依赖。
