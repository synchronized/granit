<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-33：开发与发布流水线提速

## 状态

**已完成。** 本任务只调整测试、缓存、验收和发布流程，不改变公共 API、ABI 或 Granit 版本号。
详细结果见 [S-33 流水线提速验收记录](../records/2026-09-07-s33-ci-release-acceleration.md)。

## 背景与目标

0.17.0 验收期间，多轮完整矩阵重复构建了相同依赖和产物。S-33 将高频开发检查与低频发布验收
分层，并让正式 Release 晋级已经在同一提交上验证的不可变候选产物。

## 非目标

- 不恢复 S-16 已删除的桌面 Dawn 后端或依赖。
- 不降低发布矩阵、测试、安装审计和校验要求。
- 不自动移动或覆盖已公开标签和 Release 产物。
- 不修改公共能力位来描述特定 CI 软件适配器的驱动缺陷。

## 已确认决策

- `Quick Check` 由维护者手动触发，可自动按相对 `main` 的路径选择文档、头文件、核心或 WebGPU
  编译检查，也允许显式覆盖。
- Emscripten 正式浏览器测试固定验证软件适配器异步 Pipeline 已知失败后的同步创建与渲染闭环；
  未知错误仍然失败。
- Emscripten SDK/ports、锁定 Shader 工具链和 C/C++ 编译结果分别缓存；缓存身份包含平台、版本或
  配置摘要，不缓存已经移除的桌面 Dawn。
- 手动 `Release` 运行负责构建候选、测试、安装审计、校验和生成 manifest；标签运行只接受
  tag、commit 与 manifest 完全一致的成功候选产物。

## 实施顺序

1. 增加软件 WebGPU Pipeline 降级回归断言。
2. 增加按改动范围选择的快速检查工作流。
3. 统一 Emscripten、Shader 工具链及 Release 编译缓存。
4. 增加版本验收模板并更新发布指南。
5. 改造 Release Candidate manifest 与标签晋级流程。
6. 在独立分支验证 Quick Check、Emscripten 和候选/标签发布演练。

## 测试与验收

- Workflow YAML 通过语法检查，文档检查和 `git diff --check` 通过。
- `Quick Check` 的五种输入均能得到确定检查范围，`auto` 不静默跳过未知源码改动。
- Emscripten 软件适配器降级路径通过，真实 Pipeline 创建和渲染仍是最终成功条件。
- Candidate 产物包含四套 SDK、`SHA256SUMS` 和记录 tag、commit、run ID 的 manifest。
- 标签运行找不到完全匹配候选时必须失败；匹配时不重复构建，校验后发布同一批字节。

## 风险与未决问题

- GitHub Actions Artifact 有保留期限，过期候选必须重新运行，不允许绕过 manifest 校验。
- 首轮正式晋级应使用未公开的演练标签验证；公开版本标签仍遵守不可移动规则。
- 标签晋级路径将在下一个未发布版本首次使用；当前已经验证候选生成、manifest、校验文件与四套
  SDK，不能使用已有公开标签进行破坏性演练。
