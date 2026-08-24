<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 发布验收

本文是 Granit 维护者执行预发布和稳定发布的检查清单。当前 `0.2.0` 不承诺稳定 ABI；
完成本清单也不能替代明确的稳定 component 决策。

## 1. 确认发布身份

- 在根 `CMakeLists.txt` 设置唯一的 `project(VERSION)`。
- 确认生成头中的 `GRANIT_VERSION_*` 和运行时 `granit_version_*` 一致。
- 在根 [`CHANGELOG.md`](../../CHANGELOG.md) 把本次内容从 `Unreleased` 移入带日期的版本章节。
- 预发布版本继续保留 README 的 0.x 警告；稳定发布才按已批准承诺修改措辞。

## 2. 逐项声明 component

发布说明必须把 component 分为“稳定”“实验性”“未包含”，不能只声明整个包稳定。至少逐项检查：

| component | 当前 0.2.0 状态 | 稳定发布前证据 |
|---|---|---|
| Core | 未冻结 | 正式 ABI 快照、C 契约、共享/静态 Consumer |
| RenderPipeline | 未冻结 | 正式 ABI 快照、component 契约、安装 Consumer |
| Window | 未冻结 | 正式 ABI 快照、平台矩阵、component 契约 |
| Input | 未冻结 | 正式 ABI 快照、平台矩阵、component 契约 |
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

随后安装两种链接模式，并运行 `cmake/check_install_exports.cmake`、
`cmake/check_installed_package.cmake` 和 `tests/consumer`。稳定发布不得跳过失败测试、关闭警告或降低
验证等级。

## 4. ABI 与包审计

- 比较当前核心 ABI 与上一个稳定快照，分别列出兼容新增、破坏性变化和平台差异。
- 为本次宣布稳定的可选 component 建立带版本、平台、架构和编译器身份的独立快照。
- 确认公共头文件不包含 Vulkan，安装导出不泄漏源码路径、测试库或私有依赖。
- 确认 CMake component 名、目标名、静态宏传播和运行时库部署方式与发布说明一致。

## 5. 发布说明与产物

发布说明至少列出版本、日期、稳定 component、实验性接口、已知限制、破坏性变化和迁移步骤。
发布标签必须指向完整通过验收的提交；产物应来自该标签的干净构建，不使用开发机已有构建目录。

仓库的 `Release` Actions 支持手动输入标签进行预验证，也会在推送 `v*` 标签时执行正式发布。
工作流先校验标签与 `project(VERSION)` 一致，再构建 Windows/Linux 的共享库和静态库安装包、运行
测试与安装审计、生成 `SHA256SUMS`；只有全部任务成功，标签触发的运行才创建 GitHub Release。

正式发布前先在 Actions 页面手动运行 `Release`，输入候选标签并下载 `release-assets` 检查内容。
预验证通过后再创建并推送同名标签。不要重用或移动已经公开的版本标签。

首次稳定发布只有在 [S-06](../plans/S-06-compatibility-policy.md) 的稳定门槛和本清单全部满足后
才能执行。版本号、发布日期及稳定 component 仍需单独决策。
