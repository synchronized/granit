<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# CI 与验证

本文档说明 `.github/workflows/` 下各个 workflow 的职责、触发方式与覆盖矩阵，帮助判断
"一次改动应该跑哪些检查"以及"某个 preset 是否进入 CI 必过矩阵"。

## 触发机制

除发布和 CI 镜像外，**所有 workflow 都是手动触发（`workflow_dispatch`）**，不会在
`pull_request` 上自动运行。需要验证时，在 Actions 页面手动运行对应 workflow（见
[手动触发](#手动触发)），或由外部调度触发。

唯一的自动触发是：

| Workflow | 自动触发条件 |
| --- | --- |
| `release` | 推送 `v*` tag |
| `linux-ci-image` | 向 `main` 推送且改动 `.github/ci/linux/Dockerfile` |

## Workflow 清单

| Workflow | 触发 | 职责 | 平台 / 矩阵 |
| --- | --- | --- | --- |
| `quick-check` | 手动（`suite` 参数） | 按改动范围的快速检查 | Linux |
| `documentation` | 手动 | 文档相对链接与分类索引检查 | Linux |
| `linux` | 手动 | Linux 完整验证（构建、测试、安装、Consumer） | clang / gcc × shared / static |
| `windows` | 手动 | Windows 完整验证 | MSVC × shared / static |
| `emscripten` | 手动 | 浏览器 WebGPU 构建与行为验证 | Linux + Emscripten |
| `release` | `v*` tag + 手动 | 发布产物构建、校验与发布 | win / linux × shared / static |
| `shader-toolchain` | 手动（`run_windows` / `run_linux`） | 打包锁定 Shader 工具链 | win / linux |
| `linux-ci-image` | 手动 + `main` 改 Dockerfile | 构建自定义 CI Runner 镜像 | Linux |

## 各 workflow 说明

### `quick-check` —— 按改动范围的快速检查

在 `workflow_dispatch` 时通过 `suite` 参数选择范围，默认 `auto` 根据相对 `main` 的改动自动判定：

| `suite` | 触发条件（`auto` 时） | 内容 |
| --- | --- | --- |
| `webgpu` | 改动 Web 后端 / Web 示例 / `granit_web` / `emscripten.yml` | 编译 WebGPU 快速验证目标 |
| `documentation` | 仅改动 `docs/`、`README`、`CHANGELOG` 等文档 | 文档检查 |
| `headers` | 改动 `include/`、`tests/headers/`、`cmake/` | 头文件检查 |
| `core` | 其他改动或无改动 | 单元与 Smoke 测试 |

`auto` 之外的四个值都可手动指定，跳过按 diff 判定的步骤。

### `linux` / `windows` —— 平台完整验证

两者结构对称，都先**验证锁定 Shader 工具链**，再跑完整构建 → 测试 → 安装 → 独立 Consumer：

| Workflow | 编译器矩阵 | 说明 |
| --- | --- | --- |
| `linux` | `clang` / `gcc` | 完整测试（含 GPU/平台），用 `xvfb-run` 提供虚拟显示 |
| `windows` | `MSVC` | 排除 GPU/平台测试（Runner 无 Vulkan ICD）；clang/clang-cl 仅本地开发，不进 CI |

### `emscripten` —— 浏览器 WebGPU 验证

构建 WebGPU 平台验证目标与浏览器示例，上传产物后由 `browser-validation` 矩阵在浏览器中
运行行为验证。锁定 Emscripten 5.0.6，浏览器测试环境见
[浏览器 WebGPU 指南](webgpu-browser-example.md)。

### `release` —— 发布

`validate`（标签与版本一致）→ `windows-package` / `linux-package`（各 shared/static 打包并
测试）→ `checksums`（SHA-256）→ `publish`（创建 GitHub Release）→ `verify-public-release`
（从公开 Release 重新下载校验）。流程细节见[发布](release.md)。

### `shader-toolchain` —— 工具链打包（区别于验证）

注意区分两个同名概念：

| 位置 | 角色 |
| --- | --- |
| `linux.yml` / `windows.yml` 内的 `shader-toolchain` job | **消费方**：下载并校验锁定工具链，供后续构建/测试使用 |
| 独立的 `shader-toolchain.yml` | **生产方**：构建 DXC/Tint 并打包成锁定工具链产物 |

## CI 覆盖矩阵

| 平台 | 编译器 | 链接模式 | 安装 Consumer | 状态 |
| --- | --- | --- | --- | --- |
| Windows x64 | MSVC | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |
| Linux x64 | Clang | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |
| Linux x64 | GCC | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |

浏览器 WebGPU 由 `emscripten` workflow 单独验证，不进入上表的原生 Consumer 矩阵。

## Preset 与 CI 的映射

`CMakePresets.json` 中定义的 preset 并非全部进入 CI。进入 CI 必过矩阵的只有：

- `linux-clang-*`、`linux-gcc-*`（`linux` workflow）
- `windows-msvc-*`（`windows` workflow）
- `emscripten-*`（`emscripten` workflow）

以下 preset 仅用于**本地开发**，CI 不运行：

- `windows-clang-*`、`windows-clang-cl-*` —— Windows Clang/clang-cl
- `windows-vs2022-*` —— Visual Studio 生成器

## 手动触发

在仓库 Actions 页面选择对应 workflow → **Run workflow**，按需填写输入参数后触发；或用
`gh` CLI：

```sh
gh workflow run linux.yml
gh workflow run quick-check.yml -f suite=core
```

带 `suite` / `tag` 等输入参数的 workflow 在页面或 CLI 中显式指定即可。
