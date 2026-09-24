<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-24 S-56 Window Target 与 SDL3 后端本地验收

## 验收范围

本次验收覆盖 Window Backend 与 Window Target 分离、原生和 SDL3 Emscripten 多 Canvas、SDL3
桌面 Window/Input/Surface 后端、安装依赖闭包，以及 Model Viewer Desktop/Web 输入路径统一。

## 已完成结果

- `granit_window_desc` 通过末尾追加的 Target 描述支持 Canvas selector；旧结构尺寸继续表示自动
  Target，桌面不支持的 Target 明确返回不支持。
- Window System 创建时固定私有 Backend 操作表。原生 Win32、XCB、Wayland、Emscripten 与可选
  SDL3 后端继续使用同一套 Window、Input、状态查询和 Surface API。
- 原生与 SDL3 Emscripten 后端可以按不同 selector 管理多个 Canvas，并拒绝活动窗口重复绑定。
- SDL3 桌面后端以单个事件泵和 `SDL_WindowID` 路由多窗口事件与输入；同一进程只允许一个活动
  SDL3 Window System。
- Model Viewer Desktop 改用 `granit::window` SDL3 后端，Desktop/Web 共用 Viewer 私有输入累积器；
  原 SDL 和 DOM 专用输入状态机已经删除。
- SDL3 Window Backend 与外部 `IntegrationSDL3` 保持独立：前者拥有窗口生命周期并统一输入，
  后者只把应用已有的 `SDL_Window` 接到 Surface。
- Window SDL3 关闭时不会向安装包引入 SDL3；启用时共享包部署 SDL3 运行库，静态包通过 CMake
  package 解析 SDL3 最终链接依赖。

## 本地验证

- Windows Clang Debug shared：Window 平台测试、Model Viewer Core/ImGui/Desktop Shell 测试及
  Desktop Model Viewer 真实 smoke 通过。
- Windows Clang Debug static：SDL3 Window 平台测试和 Window 静态构建通过。
- Windows Clang Release 且 `GRANIT_ENABLE_WINDOW_SDL3=OFF`：Window 契约测试通过。
- shared/static 安装结果的 C11、C++20、Window 和 Input Consumer 构建通过；静态 Consumer 能解析
  SDL3 最终链接依赖。
- Emscripten Debug：Model Viewer Web 与平台 Smoke 构建通过；Chrome 完成原生和 SDL3 双 Canvas、
  重复 selector、销毁重建、输入、Resize、资产加载和资源释放验收。
- `git diff --check` 通过。

## 待远端验证

当前 Windows 主机没有可用 WSL、Docker 或 Podman，因此无法在本地补跑 Linux。Pull Request
Actions 仍需验证 Linux Clang/GCC、shared/static、XCB/Wayland、SDL3 Window 和安装 Consumer。
远端矩阵通过后，S-56 才移入完成计划索引。
