<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 发布验收

本文是 Granit 维护者执行预发布和稳定发布的检查清单。当前 0.x 不承诺稳定 ABI；
完成本清单也不能替代明确的稳定 component 决策。

## 1. 确认发布身份

- 在根 `CMakeLists.txt` 设置唯一的 `project(VERSION)`。
- 确认生成头中的 `GRANIT_VERSION_*` 和运行时 `granit_version_*` 一致。
- 在根 [`CHANGELOG.md`](../../CHANGELOG.md) 把本次内容从 `Unreleased` 移入带日期的版本章节。
- 预发布版本继续保留 README 的 0.x 警告；稳定发布才按已批准承诺修改措辞。

## 2. 逐项声明 component

发布说明必须把 component 分为“稳定”“实验性”“未包含”，不能只声明整个包稳定。至少逐项检查：

| component | 当前 0.x 状态 | 稳定发布前证据 |
|---|---|---|
| Core | 未冻结 | 正式 ABI 快照、C 契约、共享/静态 Consumer |
| RenderPipeline | 未冻结 | 正式 ABI 快照、component 契约、安装 Consumer |
| Window | 未冻结 | 正式 ABI 快照、平台矩阵、component 契约 |
| AssetTools | 实验性 | 工具链包、资产格式与安装 Consumer |
| IntegrationSDL3 / IntegrationImGui | 实验性 | 第三方版本范围、目标导出和 smoke test |

没有独立 ABI 快照的可选 component 不得在发布说明中标记为 ABI 稳定。

## 3. 验证矩阵

发布提交必须通过 GitHub Actions 的完整 Windows/Linux 矩阵。发布候选还应在干净目录执行：

```sh
cmake --preset <shared-release-preset>
cmake --build --preset <shared-release-preset>
ctest --preset <shared-release-preset>

cmake --preset <static-release-preset>
cmake --build --preset <static-release-preset>
ctest --preset <static-release-preset>
```

随后安装两种链接模式，并运行 `tests/cmake/check_install_exports.cmake`、
`tests/cmake/check_installed_package.cmake` 和 `tests/consumer`。稳定发布不得跳过失败测试、关闭警告或降低
验证等级。

当前版本候选还要检查 Window 的完整导出快照、已移除的历史 component/头/符号、普通聚合头
不会引入原生 Window 或 Surface 描述，并分别验证 Granit Window 直接创建 Surface 与 SDL3/外部
窗口的高级入口。浏览器构建和 WebGPU Smoke、Linux XCB/Wayland 与 Windows Win32 Surface
闭环均属于此版本的跨平台验收；本机无法覆盖的平台须在 Actions 中补齐后才能发布。

## 4. ABI 与包审计

- 比较当前核心 ABI 与上一个稳定快照，分别列出兼容新增、破坏性变化和平台差异。
- 为本次宣布稳定的可选 component 建立带版本、平台、架构和编译器身份的独立快照。
- 确认公共头文件不包含 Vulkan，安装导出不泄漏源码路径、测试库或私有依赖。
- 确认 CMake component 名、目标名、静态宏传播和运行时库部署方式与发布说明一致。

## 5. 发布说明与产物

发布说明至少列出版本、日期、稳定 component、实验性接口、已知限制、破坏性变化和迁移步骤。
发布标签必须指向完整通过验收的提交；产物应来自该标签的干净构建，不使用开发机已有构建目录。

仓库的 `Release` Actions 采用不可变候选晋级：手动运行负责构建 Windows/Linux 的共享库和静态库
安装包、运行测试与安装审计，并生成 `SHA256SUMS` 和 `release-manifest.json`；推送 `v*` 标签时只
下载并校验同一 commit、同一 tag 的成功候选，不再重复构建。

正式发布必须按以下顺序执行：

1. 将发布提交合并到 `main`，确认 `project(VERSION)`、Changelog 和验收记录完整。
2. 在 Actions 中选择 `main`，对该提交手动运行 `Release`，输入尚未公开的候选标签。
3. 下载 `release-assets`，检查四套 SDK、`SHA256SUMS` 和 manifest 中的 tag、commit、run ID。
4. 候选通过后，在完全相同的提交上创建并推送同名标签。
5. 标签运行重新校验版本和候选身份后，直接发布候选中的同一批 SDK 字节。

候选 Artifact 过期、提交不一致或标签不一致时，正式发布会失败，必须在目标提交上重新生成候选。
不要重用或移动已经公开的版本标签。

## 6. 发布后验证

Release 创建后，标签工作流会从公开下载地址重新取得产物，不能复用 Actions 工作目录中的文件。
自动验证包括：

1. 使用 `gh release download` 下载全部安装包和 `SHA256SUMS`。
2. 重新计算每个压缩包的 SHA-256，并逐项与 `SHA256SUMS` 比较。
3. 检查四个精确命名的安装包都存在，且每个压缩包只有一个顶层目录。

Candidate 阶段已经对同一批字节执行安装导出审计以及全部 C11/C++20 Consumer；正式阶段通过
SHA-256 证明公开字节与候选一致，因此不重复构建 Consumer。维护者仍应确认 Release 不是草稿、
标签指向 manifest 中的提交，且四个安装包与 `SHA256SUMS` 均已公开。

任一步失败都应保留标签和失败证据，修复后发布新的修订版本；不得移动已公开标签或静默替换产物。

首次稳定发布只有在 [S-06](../plans/S-06-compatibility-policy.md) 的稳定门槛和本清单全部满足后
才能执行。版本号、发布日期及稳定 component 仍需单独决策。
