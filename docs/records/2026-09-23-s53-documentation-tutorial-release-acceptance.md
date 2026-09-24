<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-23 S-53 文档、教程与发布面验收

## 验收范围

本次验收覆盖 0.30.0 的文档职责、十章教程入口、桌面与浏览器教程构建，以及正式 Release 的
shared-only 附件集合。公共 API、ABI、资产格式和静态源码构建能力没有变化。

## 已完成结果

- 新增 Frame、Frame Context、Frame Recording、Command Recorder、Swapchain 与 Render Pipeline
  的统一分层 Concept，并让对应 Reference 只保留接口契约。
- 教程 README 成为唯一章节清单；文档中心只保留系列入口，当前文档不再依赖历史 Plan 或 Record
  才能解释现行行为。
- 十章教程均链接真实源码、构建入口和可观察验收结果；顶层 CMake 使用一个 Shader Toolchain 条件
  组织 02～08，并明确区分桌面与 Emscripten 的 Tutorial 08。
- Release 工作流只构建并校验 Windows/Linux x64 shared SDK；static preset、平台 CI 和安装
  Consumer 保持不变。

## 本地验证

- `actionlint .github/workflows/release.yml`：通过。
- `cmake -DGRANIT_SOURCE_DIR="$PWD" -P tests/cmake/check_documentation.cmake`：通过，检查 276 个
  Markdown 文件，根 README 为 180 行。
- Windows Clang Debug：十个教程目标全部构建成功，`ctest -L tutorial` 的 9 个桌面 Smoke 全部
  通过。
- Emscripten Debug：Tutorial 01、08 与 10 Web 目标全部构建成功。
- Chrome WebGPU：Tutorial 01、08 行为测试通过；Tutorial 10 的多帧渲染、质量与光照切换、输入、
  Resize、Fetch、资源释放、异步 Pipeline、上传取消与缺失资源诊断通过。

## 远端与发布验证

- Pull Request #85 的 Linux、Windows、Emscripten、Quick Check 与 Documentation 全部通过。
- Release 工作流完成 Windows/Linux shared SDK 的构建、测试、安装、打包和消费端审计。
- `v0.30.0` 已发布，附件精确包含 Windows/Linux x64 shared SDK 和 `SHA256SUMS`。
- 工作流从公开 Release 重新下载全部附件并完成 SHA-256 复验。
