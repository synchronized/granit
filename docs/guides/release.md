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

发布提交必须通过 GitHub Actions 的完整 Windows/Linux 矩阵。发布前还应在干净目录执行：

```sh
cmake --preset <shared-release-preset>
cmake --build --preset <shared-release-preset>
ctest --preset <shared-release-preset>

cmake --preset <static-release-preset>
cmake --build --preset <static-release-preset>
ctest --preset <static-release-preset>
```

随后安装两种链接模式，并运行 `tests/cmake/check_install_exports.cmake`、
`tests/packaging/check_installed_package.cmake` 和 `tests/packaging/consumer`。稳定发布不得跳过失败
测试、关闭警告或降低
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
发布标签必须指向完整通过验收的提交；产物应来自最终标记提交的干净构建，不使用开发机已有构建
目录。

仓库的 `Release` Actions 采用单次受控发布：手动运行在固定的 `main` 提交上构建 Windows/Linux
共享库和静态库安装包，运行测试与安装审计，生成 `SHA256SUMS` 和 manifest；全部成功后才为该
提交创建 tag 和 GitHub Release，最后从公开 Release 重新下载同一批字节复验。

发布分为两个阶段：版本准备负责产生可审查的 Git 提交；正式发布只读取已经进入 `main` 的提交，
不会再修改源码。完整顺序如下：

```text
版本准备脚本 → 版本提交 → 合并并推送 main → 正式发布脚本
                                              ↓
                             构建/测试/打包 → tag/Release → 公开复验
```

### 推荐完整流程

以下以 PowerShell 发布 `0.27.0` 为例，可以直接按顺序执行：

```powershell
git switch main
git pull --ff-only
git switch -c release/0.27.0

.\scripts\release.ps1 0.27.0 -Commit
git push -u origin release/0.27.0
gh pr create --fill

# PR 合并后执行
git switch main
git pull --ff-only
.\scripts\publish.ps1 0.27.0
```

其中，`release.ps1 -Commit` 自动修改并提交三个版本文件，但不会推送、创建 tag 或发布；`publish`
要求本地 `main` 与 `origin/main` 完全一致。远端构建、测试和打包全部通过后，工作流才创建 tag 和
GitHub Release。Linux/macOS 使用对应的 `release.sh --commit` 和 `publish.sh`，阶段边界相同。

### 5.1 准备版本提交

`release` 脚本更新 `CMakeLists.txt`、`README.md` 和 `CHANGELOG.md`。默认模式只显示改动，不提交：

```powershell
.\scripts\release.ps1 X.Y.Z
```

```sh
bash scripts/release.sh X.Y.Z
```

确认希望由脚本自动创建 `chore: 发布 X.Y.Z` 提交时，直接在干净工作区使用：

```powershell
.\scripts\release.ps1 X.Y.Z -Commit
```

```sh
bash scripts/release.sh X.Y.Z --commit
```

自动提交前脚本会检查工作区、版本格式、diff 和 tag 与项目版本的一致性。它只暂存三个版本文件，
不会推送分支、创建 tag 或启动 Release。

推荐完整流程通过发布分支和 Pull Request 合入受保护的 `main`。允许直接推送 `main` 时，也可以在
本地 `main` 执行自动提交，检查提交后推送：

```powershell
git switch main
git pull --ff-only
.\scripts\release.ps1 X.Y.Z -Commit
git show --stat
git push origin main
```

无论采用哪种方式，正式发布前都要确认版本提交已经位于 `origin/main`。

### 5.2 正式发布

更新本地 `main`，然后执行唯一的正式发布入口：

Linux/macOS 使用：

```sh
bash scripts/publish.sh X.Y.Z
```

PowerShell 使用：

```powershell
.\scripts\publish.ps1 X.Y.Z
```

`publish` 脚本要求当前分支为 `main`、工作区干净且 `HEAD` 等于 `origin/main`。它还会校验项目版本和
tag 唯一性，然后触发 `Release` 工作流并默认等待最终结果。传入 `--no-wait` 或 `-NoWait` 可以只触发
不等待。直接从 Actions 页面运行时必须选择 `main` 并输入 `vX.Y.Z` 格式的 tag。

工作流依次完成四套 SDK 构建、测试、安装审计、校验和与 manifest；全部通过后，`publish` job 才为
同一 `GITHUB_SHA` 创建 tag 和 Release，随后执行公开下载复验。版本准备失败不会启动远端发布，远端
构建失败也不会留下 tag。

建议为 `release` Environment 配置 required reviewer，使四套 SDK 全部完成后由维护者批准 `publish`
job；未配置保护规则时该 job 自动继续。仓库还可以启用 Immutable Releases，在发布后禁止修改 tag
和资产。这两项均为 GitHub 仓库设置，不由源码隐式修改。

如果构建、测试或 `publish` 前的校验失败，修复后在新的 `main` 提交上重新运行即可，因为工作流尚未
创建 tag。若 `publish` 已创建 Release 而公开复验失败，应保留失败证据并修复发布基础设施；不得移动
已经公开的 tag 或静默替换资产。

仓库不再把手工推送 tag 作为发布入口。意外创建但没有对应 Release 的 tag 应先删除，再通过工作流
发布；已经公开的版本应发布新的修订版本。

## 6. 发布后验证

Release 创建后，同一工作流会从公开下载地址重新取得产物，不能复用 Actions 工作目录中的文件。
自动验证包括：

1. 使用 `gh release download` 下载全部安装包和 `SHA256SUMS`。
2. 重新计算每个压缩包的 SHA-256，并逐项与 `SHA256SUMS` 比较。
3. 检查四个精确命名的安装包都存在，且每个压缩包只有一个顶层目录。

打包阶段已经对同一批字节执行安装导出审计以及全部 C11/C++20 Consumer；公开复验通过 SHA-256
证明下载字节与已验证产物一致，因此不重复构建 Consumer。维护者仍应确认 Release 不是草稿、
标签指向 manifest 中的提交，且四个安装包与 `SHA256SUMS` 均已公开。

任一步失败都应保留标签和失败证据，修复后发布新的修订版本；不得移动已公开标签或静默替换产物。

首次稳定发布只有在 [S-06](../plans/S-06-compatibility-policy.md) 的稳定门槛和本清单全部满足后
才能执行。版本号、发布日期及稳定 component 仍需单独决策。
