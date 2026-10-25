<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# CI 与验证

本文档说明 `.github/workflows/` 下各个 workflow 的职责、触发方式与覆盖矩阵，以及它们
在"开发到发布"流程中的位置，帮助判断"一次改动应该跑哪些检查"以及"某个 preset 是否进入
CI 必过矩阵"。

## 开发到发布的工作流

Granit 采用主干开发（GitHub Flow）：feature 分支通过 PR 合并到 `main`，发布工作流在全部验证
通过后为该提交创建 tag 和 Release。各 workflow 落在流程的不同验证层级：

| 阶段 | 动作 | 相关 workflow / 脚本 |
|---|---|---|
| 开发 | feature 分支写代码，按需手动验证对应平台 | `linux` / `windows` / `emscripten`（按需） |
| PR 门禁 | 开 PR，按 diff 快速检查 | `quick-check`（`suite=auto`） |
| 合并 | PR 通过后合并到 `main` | — |
| 发布准备 | 升级版本号并创建可审查的版本提交 | `release.ps1 -Commit` / `release.sh --commit` |
| 完整验证 | 发布前跑全平台矩阵 | `linux` + `windows` + `emscripten` |
| 发布 | 构建并验证 SDK，随后创建 tag 和公开 Release | `scripts/publish.* <version>` |

```text
feature 分支 ──PR/CI──> main ──创建 release 分支──> release.* --commit
                                                     │
                                              PR/完整验证
                                                     │
main <───────────────────────────────────────────────┘
 │
 └── publish.*（构建、验证、创建 tag 和 Release）
```

三个验证层级：`quick-check` 是 PR 的快速反馈，`linux`/`windows`/`emscripten` 与
`compatibility` 是手动验收入口，`release` 是手动触发的正式发布。各 workflow 的配置见下文，
发布细节见[发布验收](release.md)。

## 触发机制

PR 打开或更新时只自动运行 `quick-check`，按差异选择代码或文档快速反馈。完整矩阵仅在关键
版本或受影响的平台改动时手动运行，不绑定 push、tag 或定时任务。各 workflow 的触发方式：

| Workflow | 自动触发 | 手动触发 |
| --- | --- | --- |
| `quick-check` | `pull_request` | `workflow_dispatch`（`suite` 参数） |
| `linux` / `windows` / `emscripten` | — | `workflow_dispatch` |
| `compatibility` | — | `workflow_dispatch`（`suite` 参数） |
| `release` | — | `workflow_dispatch`（正式发布） |
| `linux-ci-image` | 向 `main` 推送且改动 Dockerfile | `workflow_dispatch` |
| `documentation` / `package-shader-toolchain` | — | `workflow_dispatch` |

分支保护的常规 required check 应使用 Quick Check。不要把全部手动完整矩阵设为每次 PR 的
必需检查；关键版本按验收记录和发布流程审查所需的手动结果。

## Workflow 清单

| Workflow | 触发 | 职责 | 平台 / 矩阵 |
| --- | --- | --- | --- |
| `quick-check` | PR + 手动（`suite` 参数） | 按改动范围的快速检查 | Linux |
| `documentation` | 手动 | 文档相对链接与分类索引检查 | Linux |
| `linux` | 手动 | Linux 完整验证（构建、测试、安装、Consumer） | clang / gcc × shared / static |
| `windows` | 手动 | Windows 完整验证 | MSVC × shared / static |
| `emscripten` | 手动 | 浏览器 WebGPU 构建与行为验证 | Linux + Emscripten |
| `compatibility` | 手动 | 多版本工具链、Debug 与 Sanitizer | Windows / Linux |
| `release` | 手动 | 发布产物构建、校验、tag 与 Release | win / linux × shared |
| `package-shader-toolchain` | 手动（`run_windows` / `run_linux`） | 打包锁定 Shader 工具链 | win / linux |
| `linux-ci-image` | 手动 + `main` 改 Dockerfile | 构建自定义 CI Runner 镜像 | Linux |

## 各 workflow 说明

### `quick-check` —— 按改动范围的快速检查

在 `workflow_dispatch` 时通过 `suite` 参数选择范围，默认 `auto` 根据相对 `main` 的改动自动判定：

| `suite` | 触发条件（`auto` 时） | 内容 |
| --- | --- | --- |
| `webgpu` | 改动 Web 后端 / Web 示例 / `granit_web` / `emscripten.yml` | 编译 WebGPU 快速验证目标 |
| `documentation` | 仅改动 `docs/`、`README`、`CHANGELOG` 等文档 | 文档检查 |
| `headers` | 改动 `include/`、`tests/contracts/headers/`、`cmake/` | 头文件检查 |
| `core` | 其他改动或无改动 | 单元与 Smoke 测试 |

`auto` 之外的四个值都可手动指定，跳过按 diff 判定的步骤。

### `linux` / `windows` —— 平台完整验证

两者结构对称，都先**验证锁定 Shader 工具链**，再跑完整构建 → 测试 → 安装 → 独立 Consumer：

| Workflow | 编译器矩阵 | 说明 |
| --- | --- | --- |
| `linux` | `clang` / `gcc` | 完整测试（含 GPU/平台），用 `xvfb-run` 提供虚拟显示 |
| `windows` | `MSVC` | 排除 GPU/平台测试（Runner 无 Vulkan ICD）；Clang 由独立 compatibility 矩阵覆盖 |

### `emscripten` —— 浏览器 WebGPU 验证

构建 WebGPU 平台验证目标与浏览器示例，上传产物后由 `browser-validation` 矩阵在浏览器中
运行行为验证。锁定 Emscripten 5.0.6，浏览器测试环境见
[浏览器 WebGPU 指南](webgpu-browser-example.md)。

### `release` —— 发布

手动运行一次执行 `validate` → `windows-package` / `linux-package`（各构建 shared SDK 并测试）→
`checksums`（SHA-256）→ `publish`（为同一提交创建 tag 和 GitHub Release）→
`verify-public-release`（从公开 Release 重新下载校验）。`publish` 可以通过 GitHub 的 `release`
Environment 增加人工审批。完整命令与失败恢复见[发布验收](release.md)。

### `package-shader-toolchain` —— 工具链打包（区别于验证）

注意区分生产与消费两方：

| 位置 | 角色 |
| --- | --- |
| `linux.yml` / `windows.yml` 内的 `restore-shader-toolchain` job | **消费方**：下载并校验锁定工具链，供后续构建/测试使用 |
| 独立的 `package-shader-toolchain.yml` | **生产方**：构建 DXC/Tint 并打包成锁定工具链产物 |

## CI 覆盖矩阵

| 平台 | 编译器 | 链接模式 | 安装 Consumer | 状态 |
| --- | --- | --- | --- | --- |
| Windows x64 | MSVC | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |
| Linux x64 | Clang | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |
| Linux x64 | GCC | 共享、静态 | C11、C++20、RenderPipeline、Window（含输入） | Release CI 已通过 |

浏览器 WebGPU 由 `emscripten` workflow 单独验证，不进入上表的原生 Consumer 矩阵。

## Preset 与 CI 的映射

`CMakePresets.json` 中定义的 preset 并非全部进入平台完整验收。现有平台工作流使用：

- `linux-clang-*`、`linux-gcc-*`（`linux` workflow）
- `windows-msvc-*`（`windows` workflow）
- `emscripten-*`（`emscripten` workflow）

以下 preset 仅用于**本地开发**，CI 不运行：

- `windows-clang-*`、`windows-clang-cl-*` —— 本地 preset；独立 compatibility 使用显式编译器构建目录
- `windows-vs2022-*` —— Visual Studio 生成器

## 手动触发

在仓库 Actions 页面选择对应 workflow → **Run workflow**，按需填写输入参数后触发；或用
`gh` CLI：

```sh
gh workflow run linux.yml
gh workflow run quick-check.yml -f suite=core
bash scripts/publish.sh X.Y.Z
```

`publish` 脚本会校验本地与远端 `main`、版本和标签，并默认等待工作流完成。

## 本地统一验证入口

`scripts/dev.ps1 <preset> -Test` 或 `scripts/dev.sh <preset> --test` 配置、构建并执行
默认测试集合；`-Suite quick` / `--suite quick` 选择不依赖 GPU 和窗口的单元及 Smoke 测试。
报告保存在 `build/validation/<UTC 时间与随机编号>/`，终端打印绝对路径。

报告包含源码提交与工作区状态、实际工具链配置、测试清单、配置/构建/测试命令、原始日志和
CTest JUnit 结果。`summary.json` 区分 `passed`、`failed` 和存在跳过项的 `incomplete`；
没有选中测试视为失败。薄入口保留子命令的数值失败码，例如 CTest 失败为 8；
无法表示的平台异常返回通用失败码，原始结果保存在报告中。

普通 test preset 按 `_stress` 名称后缀排除长测，注册函数强制该命名。
显式长测入口覆盖名称排除；quick 再按标签排除 GPU、平台与长测。
不要把默认保护改为额外的标签排除：CTest 的多个 `-LE` 条件按 AND 合并，
会令 quick 的排除条件失效。
配置失败时仍保留环境与配置日志，但不会声称已经获得实际编译器身份或测试结果。

## 关键版本兼容验收

`compatibility.yml` 使用 `.github/ci/compatibility-matrix.json` 作为工具链组合的唯一清单，
原生组合验证共享/静态 Release、版本边界与代表性 Debug；Sanitizer 使用独立 Linux Clang
构建，ASan+UBSan 覆盖适用测试，TSan 限定 CPU 句柄、异步状态与回收队列。

Windows LLVM 锁定完整发行版本；MSVC 选择工具集系列；Linux 使用指定大版本软件包。
后两者的补丁版本随安装源变化，报告记录实际完整版本，不能称为逐字节锁定的编译器环境。
这些是待验收的版本范围，最低支持结论必须以对应提交的成功记录为依据。

手动运行示例：`scripts/ci.ps1 compatibility -Ref <候选分支> -Suite all`，或
`scripts/ci.sh compatibility <候选分支> all`。`native` 与 `sanitizers` 可分别执行。
脚本通过已有 Linux 手动入口的 `validation=compatibility` 调用同提交的兼容工作流，
避免新工作流尚未进入默认分支时直接调度返回 404。复用定义的两个任务均校验原始事件
必须为 `workflow_dispatch`，不能通过 PR、push 或定时调用启动矩阵。
复用调用的上下文规则见 [GitHub 官方说明](https://docs.github.com/en/actions/reference/workflows-and-actions/reusing-workflow-configurations)。
Web 回归使用同一提交的 Emscripten 工作流，避免将不同提交的结果拼成通过证据。
矩阵报告在 Actions 的 `compatibility-<组合>` 制品中，Windows 报告明确标为 `incomplete`，
因为云端不执行完整 GPU/平台用例，需要本地真实 GPU 证据补足。

公共 API/ABI、安装导出、依赖或工具链升级、平台生命周期变化及稳定候选里程碑需要关键版本验收。
普通补丁按影响范围选择；不要求每次 PR 或每个补丁全量运行。验收后修改受影响代码必须重新验证。

## Linux 虚拟机测试环境

没有 GPU 直通的虚拟机可使用 Mesa 软件 Vulkan 验证资源生命周期、回读和窗口流程。
这类结果必须注明 CPU 软件设备，不能替代真实 GPU 与驱动验收。
Arch Linux 的基础环境可安装以下官方软件包：

```sh
sudo pacman -S --needed vulkan-icd-loader vulkan-swrast vulkan-tools \
  vulkan-validation-layers xorg-server-xvfb xorg-xauth \
  libx11 libxcursor libxi libxrandr libxinerama libxss libxtst \
  libxkbcommon libxkbcommon-x11 libxdamage libxcomposite libxrender \
  libxfixes libxext libxcb wayland wayland-protocols weston
vulkaninfo --summary
xvfb-run -a vulkaninfo --summary
```

`vulkan-swrast` 提供软件 Vulkan，见 [Arch Vulkan 指南](https://wiki.archlinux.org/title/Vulkan)。
确认输出包含 Khronos Validation Layer，并记录设备名称、驱动、包版本和实际编译器身份。
Xvfb 提供 X11 显示；Wayland 测试另需启动无头 Weston，不能以 X11 通过代替 Wayland 通过。
软件设备是否满足 Granit 要求仍以项目测试结果为准。

## 本地真实 GPU 长测

长测默认不注册。使用 `scripts/dev.ps1 windows-clang-release -Suite stress -Seconds 3600`
或 `scripts/dev.sh linux-clang-release --suite stress --seconds 3600` 显式启用、构建并运行。
`-Seed` / `--seed` 指定可复现随机种子，默认 20261024；`-Iterations` / `--iterations`
限制单项迭代数，默认 0 表示仅按时长停止，达到任一边界便结束。短时自检可设 10 秒，
扩展验收设 28800 秒。
时长按每项测试计算，三项串行的一小时配置共约三小时，八小时配置共约二十四小时。

用例覆盖资源上传/回读和旧句柄失效、窗口/Surface/Swapchain 反复创建重建，以及同进程持续
渲染、材质切换与逐帧回读。GPU 资源锁限制 CTest 并发，每项超时额外留 120 秒清理余量。
Windows 长测创建隐藏窗口以免反复抢占桌面；窗口用例改变新窗口尺寸，同窗口最小化与原生
尺寸事件不在本次自动长测覆盖内。不要同时启动多个 GPU 验证进程，CTest 资源锁仅在单次
CTest 调度内部生效。长测应使用独立终端或独立后台进程，关闭会话可能中断测试。

心跳每十秒记录时长、迭代、种子、进程 RSS、诊断计数和区间平均迭代耗时；崩溃、超时、错误回读、非预期验证或
生命周期诊断均失败。资源负向测试只接受该调用预期的无效句柄诊断。内存趋势记录不设置性能阈值，
需要在同一环境比较预热后的采样；显存趋势目前没有统一采集接口，不能从 RSS 推断。

Ctrl+C/SIGTERM 请求在迭代边界清理并以失败结束；CTest 超时和强制终止可能只保留部分日志。
CTest 重复启动进程适合偶发回归，不能代替上述同进程长测。

如果本机第三方 Overlay 的隐式 Layer 注册已损坏，可以为本次测试进程设置
`VK_IMPLICIT_LAYER_PATH` 指向专用空目录，保留显式 Khronos Validation Layer 搜索路径。
该机制见 [Vulkan Loader 文档](https://github.com/KhronosGroup/Vulkan-Loader/blob/main/docs/LoaderLayerInterface.md)。
这是隔离环境的验收，必须在记录中说明，不能当成 Overlay 兼容性已通过。
验证层是否实际可用需结合运行环境记录核实；仅请求 Validation 不构成已启用证明。

报告的通过/跳过统计以 CTest 测试进程为单位；Catch2 进程内部的条件跳过需同时审查
JUnit 中的测试输出，不能据绿色退出码宣称所有硬件能力均已验证。

Windows GNU 风格 Clang preset 显式使用 LLVM 自带的 `llvm-rc`，避免把 Clang 预处理的
GNU 行标记交给 Windows SDK `rc.exe`。自定义编译器路径时，应同时选择匹配的资源编译器。

区间平均耗时包含 CPU 工作与等待，不代表 GPU Timestamp。Windows Clang 旧构建目录切换资源
编译器时应先用 `scripts/msvc.ps1 -Action configure -Preset <preset> -Fresh` 重建配置；
CMake 因编译器变化自动清缓存后可能选择 PATH 默认编译器，应核对实际工具链环境报告。
