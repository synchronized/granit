<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Example Asset Manager 契约

本目录提供仓库示例共用的只读资产入口。它用于让 Desktop 和 Web 示例共享业务代码，属于
`examples/common` 私有实现，不安装、不导出，也不是 Granit 公共 VFS。

## 调用入口

业务代码只提交稳定意图和逻辑位置：

- `asset_location::bundled(path)` 表示随示例部署的只读资产；
- `asset_location::external(location)` 表示用户文件或 HTTP(S) URL；
- `asset_manager::load<T>()` 立即返回强类型 `asset_handle<T>`，不会阻塞调用线程；
- `asset_group` 可汇总一批根请求及其依赖进度。

同一个 bundled 逻辑路径在 Desktop 从可执行文件旁的 `assets` 目录读取，在 Web 从预加载的
`/assets` 读取。外部位置在 Desktop 走文件 Source，在 Web 可走 MEMFS 或 Fetch。平台组合入口显式
注册 Source，业务代码不选择后端，也不创建 Mount。

Application Host 拥有 Task System 和 Asset Manager，并在每个 Tick 发布 main completion。即使
Source 或 Executor 可以内联完成工作，Handle 也不会在 `load<T>()` 返回前发布 ready/failed 终态。

## 复合资产

glTF Loader 发现 Buffer 和图片 URI；Manager 相对于主文档位置解析、读取并汇总依赖，所有依赖
就绪后才发布完整 CPU Scene。调用方不构造 Resolver，也不手工轮询依赖。

Shader Library 由构建过程生成并按目标选择嵌入或安装位置，它具有自身格式和加载接口，不作为普通
模型、纹理 Blob 交给 Example Asset Manager 管理。

## 部署

示例目标通过 `granit_example_target_assets(TARGET ... ROOT ... FILES ...)` 声明随程序发布的文件。
`FILES` 必须是 `ROOT` 下的规范相对路径：

- Desktop 构建后复制到目标可执行文件旁的 `assets/<logical-path>`。
- Emscripten 以同一个逻辑路径预加载到 `/assets/<logical-path>`。

教程提交的资产在各自 `examples/assets/.../README.md` 中记录来源、许可证、固定上游版本和
SHA-256。完整 Model Viewer 资产使用 manifest 下载与校验，不把大型外部模型提交到仓库。

只有出现至少两个仓库外使用者，并且位置、异步完成、取消、缓存和线程语义稳定后，才另行评估
公共资产 API；当前代码不能被公共头文件或安装目标依赖。
