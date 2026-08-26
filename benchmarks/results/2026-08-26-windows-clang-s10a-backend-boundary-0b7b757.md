<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10A Windows 后端边界回归

## 验证范围

- 提交：`0b7b757`
- 构建：`windows-clang-release`，共享库
- 测试：Release 全量 50 项测试通过
- 负载：1280×720、Immediate、三帧槽、关闭 Validation、完整 ImGui Demo 与自定义纹理
- 采样：60 帧预热，300 帧原始样本

## 结果

单位均为毫秒。

| 指标 | p50 | p95 |
|---|---:|---:|
| 全帧 | 0.459 | 0.532 |
| Acquire | 0.283 | 0.316 |
| Canvas Record | 0.038 | 0.059 |
| Submit | 0.025 | 0.035 |
| GPU | 0.448 | 0.451 |

与 [F-12B 帧循环性能基线](2026-08-25-windows-clang-frame-loop-c187748.md) 相比，Canvas Record、
Submit 和全帧时间均未出现数量级回退。GPU 与 Acquire 的等待分布受驱动、桌面负载及提交排队位置
影响，不能单独据此归因于后端抽象。当前数据不支持“Swapchain/Queue 虚接口增加了可测量帧开销”
这一判断。

## 复现命令

```powershell
cmake --preset windows-clang-release
cmake --build --preset windows-clang-release
ctest --preset windows-clang-release --output-on-failure
.\build\windows-clang-release\bin\granit_sdl3_imgui_example.exe `
  --present-mode immediate --frames-in-flight 3 --no-validation `
  --profile-warmup 60 --profile-frames 300 `
  --profile-output build/windows-clang-release/s10a-imgui-immediate-3-off.csv
```

原始 CSV 保留在本地构建目录，不提交生成数据。Linux 共享与静态构建仍需通过手动 Actions 验证。
