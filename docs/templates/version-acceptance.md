<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Granit 版本验收模板

复制本模板到对应版本 Plan，并在版本开始时确定范围。Plan 只保留目标和状态；详细执行证据在完成后
写入 `docs/records/`。

## 功能完成条件

- [ ] 公共 C API、C++ 包装和所有权语义已同步。
- [ ] Vulkan 与 WebGPU 实现或明确的能力降级已经完成。
- [ ] 错误、取消、生命周期和诊断路径已经覆盖。
- [ ] Reference、Guide、迁移说明和 Changelog 已更新。

## 验证矩阵

- [ ] Windows shared/static。
- [ ] Linux shared/static。
- [ ] C11/C++20 公共头、ABI 快照和安装 Consumer。
- [ ] Emscripten 构建与 Chrome WebGPU 验收。
- [ ] Documentation 与 `git diff --check`。
- [ ] Release Candidate 四套 SDK、安装审计、manifest 和 SHA-256 校验。

## 发布条件

- [ ] 根项目版本、生成版本与候选标签一致。
- [ ] 计划状态、路线图摘要和发布验收记录一致。
- [ ] Candidate 基于 `main` 的目标提交，且工作区干净。
- [ ] 标签指向 manifest 记录的同一提交。
- [ ] 正式 Release 直接晋级已验证候选，不重新构建。
- [ ] Release 非草稿，四套 SDK 与 `SHA256SUMS` 均可公开下载并通过复验。

## 未验证项与风险

- [ ] 所有未验证平台、硬件路径和已知限制均已明确记录。
