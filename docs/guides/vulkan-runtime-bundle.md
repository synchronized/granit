<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Vulkan Runtime Bundle 部署

## 适用场景

本指南说明如何让安装后的 Granit 应用使用 v0.47.0 Vulkan Runtime Bundle。Bundle 只包含 Vulkan
Loader 和 Khronos Validation Layer，不包含显卡驱动、ICD、Vulkan SDK 或 Vulkan Headers。

下载入口：[Granit v0.47.0 Release](https://github.com/synchronized/granit/releases/tag/v0.47.0)。

| 平台 | 文件 |
|---|---|
| Windows x64 | `granit-vulkan-runtime-windows-x64.zip` |
| Linux x64 | `granit-vulkan-runtime-linux-x64.tar.zst` |

`SHA256SUMS` 与两个 Runtime Bundle 一起发布，部署前应先校验下载文件。

## 前置条件

- 已安装对应平台的 Vulkan 显卡驱动和 ICD；
- 已完成 Granit SDK 的 CMake 配置和链接；
- 应用使用与目标平台匹配的 x64 Runtime Bundle；
- 应用进程没有在 Granit 初始化前自行加载另一个 Vulkan Loader。

没有 Vulkan 驱动或 ICD 时，Runtime Bundle 不能提供 GPU，也不会把机器伪装成支持 Vulkan。

## 操作步骤

### 1. 下载和校验

Linux：

```sh
runtime_url=https://github.com/synchronized/granit/releases/download/v0.47.0/
curl -LO "${runtime_url}granit-vulkan-runtime-linux-x64.tar.zst"
curl -LO https://github.com/synchronized/granit/releases/download/v0.47.0/SHA256SUMS
sha256sum --check SHA256SUMS --ignore-missing
zstd -d granit-vulkan-runtime-linux-x64.tar.zst
tar -xf granit-vulkan-runtime-linux-x64.tar
```

Windows PowerShell：

```powershell
$runtime_url = "https://github.com/synchronized/granit/releases/download/v0.47.0/"
Invoke-WebRequest `
  -Uri "${runtime_url}granit-vulkan-runtime-windows-x64.zip" `
  -OutFile granit-vulkan-runtime-windows-x64.zip
Get-FileHash granit-vulkan-runtime-windows-x64.zip -Algorithm SHA256
Expand-Archive granit-vulkan-runtime-windows-x64.zip -DestinationPath .
```

Windows 的 SHA-256 应与 Release 中 `SHA256SUMS` 对应行一致。

### 2. 部署 Loader

Windows 将 `bin/vulkan-1.dll` 放在应用可执行文件同一目录：

```text
MyApp/
  MyApp.exe
  granit.dll
  vulkan-1.dll
  validation/
```

Linux 将 `lib/libvulkan.so.1` 放在可执行文件目录的上一级 `lib/`：

```text
MyApp/
  bin/my_app
  lib/libvulkan.so.1
  validation/
```

Granit 默认使用 `auto`：先尝试系统 Loader，系统 Loader 不可用时再尝试上述应用私有 Loader。
应用不需要复制或修改系统目录。

### 3. 选择 Loader 模式

```text
GRANIT_VULKAN_RUNTIME=system    只使用系统 Loader
GRANIT_VULKAN_RUNTIME=bundled   只使用应用私有 Loader
GRANIT_VULKAN_RUNTIME=auto      系统优先，失败后使用应用私有 Loader
GRANIT_VULKAN_RUNTIME=none      禁止 Loader 初始化
```

开发和诊断时可以指定完整 Loader 路径：

```powershell
$env:GRANIT_VULKAN_LOADER_PATH = "C:\path\to\vulkan-1.dll"
```

```sh
export GRANIT_VULKAN_LOADER_PATH=/path/to/libvulkan.so.1
```

显式路径优先于 `GRANIT_VULKAN_RUNTIME` 的自动选择。Release 应优先使用固定目录布局和
`auto`，不要依赖开发机环境变量。

### 4. 开启 Validation Layer

Release 默认不要求 Validation Layer。开发期可以配置：

```text
GRANIT_VULKAN_VALIDATION=off    强制关闭
GRANIT_VULKAN_VALIDATION=auto   请求时启用，缺失时继续运行
GRANIT_VULKAN_VALIDATION=on     强制启用，缺失时初始化失败
```

`GRANIT_VULKAN_VALIDATION_PATH` 必须指向包含 `VkLayer_khronos_validation.json` 的 `validation/`
目录：

```powershell
$env:GRANIT_VULKAN_VALIDATION=on
$env:GRANIT_VULKAN_VALIDATION_PATH="$PWD\validation"
.\MyApp.exe
```

```sh
export GRANIT_VULKAN_VALIDATION=on
export GRANIT_VULKAN_VALIDATION_PATH="$PWD/validation"
./bin/my_app
```

`auto` 只在应用请求验证时启用；`on` 会强制要求验证层和 Debug Utils 扩展可用。

## 失败诊断

| 现象 | 优先检查 |
|---|---|
| `backend unavailable` | 显卡 Vulkan 驱动、ICD、Loader 模式和 Bundle 架构 |
| `auto` 没有使用 Bundle | Bundle 是否位于 exe 同目录（Windows）或 `../lib`（Linux） |
| 验证层找不到 | `GRANIT_VULKAN_VALIDATION_PATH` 是否指向 `validation/`，JSON 与动态库是否成套 |
| `on` 初始化失败 | 验证层版本、驱动版本和 Vulkan 设备扩展是否匹配 |
| Windows DLL 加载失败 | 确认使用 x64 Bundle，且 `vulkan-1.dll` 与 exe 同目录 |
| Linux Loader 加载失败 | 确认文件名为 `libvulkan.so.1`，并检查 Bundle 的平台架构 |

Runtime Bundle 只能解决 Loader 的分发问题。驱动、ICD、窗口系统和设备特性仍由目标平台提供，
Validation Layer 的错误也不能通过切换 Bundle 来修复。

## 最小验证

先在不设置任何环境变量的情况下运行应用，确认系统 Loader 路径正常；再设置
`GRANIT_VULKAN_RUNTIME=bundled` 验证应用私有 Loader；最后设置
`GRANIT_VULKAN_VALIDATION=on` 和 `GRANIT_VULKAN_VALIDATION_PATH` 验证开发期诊断。

三次运行都应记录 Renderer 初始化结果和 Granit Diagnostic 输出，不要只根据进程是否启动判断
Vulkan 后端可用。
