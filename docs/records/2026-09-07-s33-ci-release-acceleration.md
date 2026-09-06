<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-07 S-33 开发与发布流水线提速验收

## 结论

S-33 已完成。开发检查、后端集成验证和完整发布矩阵已经分层；候选产物只构建一次，正式标签将
通过 manifest 晋级同一批字节。该任务未修改公共 API、ABI 或 Granit 版本号。

## 验收范围

- `Quick Check` 的自动分类、文档和 WebGPU 显式入口均在 `main` 上成功运行。
- Emscripten 正式测试验证软件适配器异步 Pipeline 失败后的同步降级及真实渲染闭环。
- Release Candidate 的 Windows/Linux、动态/静态四套 SDK 均完成构建、测试和安装审计。
- 候选包含四套 SDK、`SHA256SUMS` 和记录 tag、commit、run ID 的 manifest。
- 同一提交第二次构建时，Windows `sccache` 与 Linux `ccache` 的四个矩阵均达到 100% 命中。

## 远端结果

- [Quick Check 自动分类](https://github.com/synchronized/granit/actions/runs/34045625920)：成功。
- [Quick Check 文档入口](https://github.com/synchronized/granit/actions/runs/34045631223)：成功。
- [Quick Check WebGPU 入口](https://github.com/synchronized/granit/actions/runs/34045636291)：成功。
- [Emscripten 集成验证](https://github.com/synchronized/granit/actions/runs/34044630280)：成功。
- [Release Candidate 完整矩阵](https://github.com/synchronized/granit/actions/runs/34044709821)：成功。
- [Release Candidate 缓存复验](https://github.com/synchronized/granit/actions/runs/34045074120)：成功。

## 已知边界

标签晋级不使用已经公开的 `v0.17.0` 做破坏性演练。下一个未发布版本创建正式标签时，仍必须
满足 tag、commit、候选 manifest 和 SHA-256 完全一致；找不到匹配候选时发布应直接失败。
