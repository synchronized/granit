<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Benchmark 结果

本目录保存具备明确提交、环境、参数和重复次数的基线摘要。原始 CSV 应在本地或 CI 产物中保留；
仓库只提交足以解释结论和后续对比的摘要，避免把单台机器的数据当作跨平台性能承诺。

结果文件使用 `YYYY-MM-DD-平台-主题-提交.md` 命名。任何优化结论必须与相同环境和参数下的既有
基线比较，并说明波动范围。

- [Render Graph 首份性能基线](2026-08-11-windows-clang-render-graph-6bac5a5.md)
- [材质系统首份性能基线](2026-08-11-windows-clang-material-8ef28fa.md)
- [PBR Pass 首份 CPU 性能基线](2026-08-12-windows-clang-pbr-pass-6f9bf01.md)
- [Scene 多 View 首份 CPU 性能基线](2026-08-12-windows-clang-scene-fb38f6f.md)
- [Lighting 可见光源打包首份 CPU 性能基线](2026-08-13-windows-clang-lighting-cc2ef53.md)
- [Lighting 分 Pass 首份 GPU 性能基线](2026-08-13-windows-clang-lighting-gpu-eefb0d8.md)
- [Render Pipeline 与 H-05 性能对比](2026-08-14-windows-clang-render-pipeline-5c0613a.md)
- [Canvas Draw List 与动态几何首份 CPU 性能基线](2026-08-14-windows-clang-ui-056a0c8.md)
- [Canvas Pass 首份 GPU 性能基线](2026-08-14-windows-clang-ui-gpu-f77b112.md)
- [ImGui Draw Data 转换首份 CPU 性能基线](2026-08-17-windows-msvc-imgui-integration-d0890b6.md)
- [H-09B 透明覆盖层首份 GPU 性能基线](2026-08-18-windows-clang-transparent-b4b0396.md)
- [H-09D Render Pipeline 多光源曲线](2026-08-19-windows-clang-multi-light-7e14162.md)
- [H-09E Render Pipeline 绑定压力曲线](2026-08-20-windows-clang-binding-pressure-7b3e246.md)
- [F-11A Canvas 绑定 CPU/GPU 基线](2026-08-25-windows-clang-canvas-binding-f11a.md)
- [F-11C Canvas 绑定缓存结果](2026-08-25-windows-clang-canvas-binding-f11c.md)
- [F-11D Canvas 单 Rendering 区间结果](2026-08-25-windows-clang-canvas-binding-f11d.md)
- [F-11E ImGui 验证记录](2026-08-25-windows-clang-canvas-binding-f11e.md)
- [F-12B 帧循环性能基线](2026-08-25-windows-clang-frame-loop-c187748.md)
