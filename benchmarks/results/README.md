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
