<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-14 S-42 测试架构基线

## 结果

S-42A 在 Windows VS2022 Release 共享库配置下记录到 94 个 CTest，其中 14 个使用
`granit.smoke.*` 名称。现有测试执行集合没有改变；全部测试已获得至少一个职责标签，可在后续迁移
中按标签比较覆盖范围。

当前 Smoke 的历史完整运行约需 2.9 秒。主要问题是多个可执行文件重复初始化 Renderer、创建离屏
目标或维护窗口帧循环，而不是单次运行耗时。

## Smoke 基线

| 当前职责 | 测试 | 平台条件 | S-42 后续处理 |
|---|---|---|---|
| 基础查询 | `version`、`renderer` | 桌面 | 由模块测试与 GPU Smoke 覆盖后删除 |
| 离屏图形 | `offscreen_clear`、`texture_readback`、`offscreen_triangle` | 桌面 GPU | 合并为 `gpu_offscreen` |
| 独立 GPU 能力 | `compute`、`pbr_offscreen`、`render_pipeline_offscreen` | 桌面 GPU | 移入 GPU 集成测试并保留独有断言 |
| 模块场景 | `material_hot_reload`、`immediate_ui_adapter` | 桌面 GPU | 合入所属模块或集成测试 |
| 窗口管线 | `render_pipeline_window`、`window_clear` | Windows | 保留并加强一个窗口 Smoke |
| 窗口图形 | `window_triangle`、`window_hdr` | Windows | 迁移独有断言后删除 |
| 外部窗口 | `sdl3_window_clear` | Windows 且启用 SDL3 | 移入 SDL3 集成测试 |
| 原生窗口 | `xcb_window_clear`、`wayland_window_clear` | 对应 Linux 能力可用 | 合入 Window component 测试 |

Windows 默认配置实际注册前五组共 14 个 Smoke；SDL3、XCB 与 Wayland 按配置和平台替换或补充
对应窗口路径。

## 标签基线

Windows VS2022 Release 共享库配置中的标签分布为：

| 标签 | 测试数 |
|---|---:|
| `unit` | 21 |
| `smoke` | 14 |
| `gpu` | 19 |
| `integration` | 63 |
| `platform` | 5 |
| `package` | 13 |
| `release` | 13 |
| `tooling` | 41 |

标签允许重叠，因此各项之和不等于测试总数。`tooling` 是工具可执行文件与工具链脚本的辅助标签；
其中发布包契约同时带有 `package` 和 `release`。

## 验证方式

- `cmake --preset windows-vs2022-release` 配置成功。
- `ctest --show-only=json-v1` 返回 94 个测试、14 个 Smoke 和 0 个未标记测试。
- 本阶段只增加 CMake 测试属性与基线文档，没有增删测试目标、测试命令或产品代码。

后续阶段以这份名称、职责和标签基线检查迁移结果，最终目标见
[S-42 测试架构收敛计划](../plans/S-42-0.24.0-test-architecture-convergence.md)。
