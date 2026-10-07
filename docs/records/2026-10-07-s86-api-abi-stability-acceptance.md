<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-86：API/ABI 稳定候选验收

## 结论

v0.50.0 将 Core、Math、Renderer 和 Window 保持为长期候选范围，RenderPipeline 保持观察候选，
AssetTools 与第三方 Integration 保持实验性。该结论不冻结整个 0.x ABI。

## 已验证证据

- 公共 API 稳定等级与 component 边界已记录在 [API 稳定等级](../reference/api-stability.md)；
- 兼容版本、结构扩展、破坏性变更和 C++ 包装迁移规则已记录在[兼容策略](../reference/compatibility.md)；
- Windows Clang 本地 `granit.contracts.api_stability`、`granit.abi.exports` 和文档链接检查通过；
- 既有 C11 布局快照、导出符号快照和安装 SDK C/C++ Consumer 继续纳入发布门禁；
- 暂缓功能复评没有发现足以恢复 Bindless、Clustered Forward、CSM、Android 或公共 glTF/Scene SDK
  的真实 Consumer 证据；
- Gneiss `UPSTREAM-043/044` 的 PBR、纹理变体上传和纹理驻留边界不发生变化。

## 发布限制

“长期候选”不是已冻结 ABI。0.x 次版本仍可能有记录地进行破坏性调整，使用者应重新编译并阅读
Changelog；C++ 二进制 ABI 和 Vulkan 原生互操作仍不在承诺范围内。
