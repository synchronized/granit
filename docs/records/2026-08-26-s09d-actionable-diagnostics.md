<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-08-26 S-09D 可执行诊断

## 结论

S-09D 已完成。诊断回调现在除 Vulkan Validation、生命周期和设备丢失外，还能在有效 Renderer
上下文中定位代表性的 Buffer 描述错误、失效句柄和跨 Renderer 句柄。程序逻辑继续只比较稳定结果
码，诊断文本不构成机器可解析协议。

## 实施范围

- 增加 Renderer 内部 validation 诊断入口，复用已有同步诊断汇聚点，不新增公共 ABI。
- `granit_buffer_create` 的空参数和无效描述会报告 API 名与 `desc`/输出参数约束。
- `granit_buffer_destroy` 的失效、类型错误或跨 Renderer 句柄会报告期望的 Buffer 类型与归属约束；
  映射中的 Buffer 会报告生命周期状态。
- 保留已有 Renderer 级联销毁诊断，覆盖生命周期问题。
- 无效 Renderer 无法定位其诊断回调，只返回 `GRANIT_ERROR_INVALID_HANDLE`；该限制已写入 Reference。

## 验证结果

- Windows Clang Debug：完整构建成功，CTest 47/47 通过。
- 安装包 C/C++ Consumer：7/7 通过；C++ Consumer 验证 validation 回调可跨共享库边界接收消息。
- 单元测试验证参数错误和跨 Renderer 句柄仍分别返回原结果码，并收到 `validation` 类别诊断。
- `git diff --check` 通过。

## 下一步

进入 S-09E，完成 0.3.0 ABI 差异、迁移说明、跨平台共享/静态安装矩阵和 Release 预验证。
