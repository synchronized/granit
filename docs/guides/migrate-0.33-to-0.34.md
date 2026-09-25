<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 从 0.33 迁移到 0.34

## 适用场景

使用实验性 AssetTools C API 直接查询 Material、Texture 或 Environment 构建结果的项目需要调整。
Core、Renderer、RenderPipeline、Window、资产格式和命令行调用不受这项变化影响。

## AssetTools 结果查询

0.34 将每个 Builder 结果收敛为一次 `result_get_info` 查询。原有多个字段 getter 已删除：

| 0.33 API | 0.34 API 与字段 |
|---|---|
| `granit_asset_tools_texture_result_get_manifest` | `texture_result_get_info` → `manifest` |
| `granit_asset_tools_texture_result_get_payload` | `texture_result_get_info` → `payload` |
| `granit_asset_tools_texture_result_get_debug_json` | `texture_result_get_info` → `debug_json` |
| `granit_asset_tools_texture_result_get_diagnostic` | `texture_result_get_info` → `diagnostic` |
| `granit_asset_tools_material_result_get_archive` | `material_result_get_info` → `archive` |
| `granit_asset_tools_material_result_get_debug_json` | `material_result_get_info` → `debug_json` |
| `granit_asset_tools_material_result_get_diagnostic` | `material_result_get_info` → `diagnostic` |
| `granit_asset_tools_environment_result_get_package` | `environment_result_get_info` → `package` |
| `granit_asset_tools_environment_result_get_debug_json` | `environment_result_get_info` → `debug_json` |
| `granit_asset_tools_environment_result_get_diagnostic` | `environment_result_get_info` → `diagnostic` |

将原来的多次查询改为初始化信息结构并查询一次：

```c
granit_asset_tools_material_result_info info = GRANIT_ASSET_TOOLS_MATERIAL_RESULT_INFO_INIT;
granit_result status = granit_asset_tools_material_result_get_info(result, &info);
if (status == GRANIT_SUCCESS) {
  use_archive(info.archive, info.archive_size);
  log_diagnostic(info.diagnostic, info.diagnostic_length);
}
```

信息结构中的指针仍由结果句柄拥有，只在句柄销毁前有效。不要释放、修改或在销毁后保存这些指针。

C++ 包装保留 `archive()`、`package()`、`manifest()`、`payload()`、`debug_json()` 和
`diagnostic()` 便利访问器；需要多个字段时优先调用一次 `info()`：

```cpp
const auto info = build_result.info();
write_bytes(info.archive);
report(info.diagnostic);
```

Shader Library 在 0.33 已采用 `result_get_info`，0.34 只为其结果信息补齐保留字段初始化，正常使用
`GRANIT_ASSET_TOOLS_SHADER_LIBRARY_RESULT_INFO_INIT` 的代码无需修改。
