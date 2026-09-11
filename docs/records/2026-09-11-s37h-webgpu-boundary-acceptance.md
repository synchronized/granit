<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-11 S-37H WebGPU Provider 边界收敛验收

## 结果

S-37H 本地实现与验证通过。浏览器 WebGPU 后端已删除 Provider ABI、运行时函数表、dispatch、
domain adapter 和冗余 owner 对象。`webgpu_renderer_state` 直接实现各 HAL 领域；后端私有设备实现
按资源、Shader、Pipeline、命令、Timestamp 和呈现拆分，共享状态集中管理 Dawn 设备生命周期、
异步回调和原生句柄表。

公共 C/C++ API、ABI 和 Vulkan 行为未改变。源码、公共头、构建脚本和测试中未发现历史 Provider
边界符号残留。

## 本地验证

- `cmake --build --preset emscripten-debug`：通过。
- `ctest --preset emscripten-debug`：3/3 通过。
- Chrome WebGPU 平台测试：多帧渲染、质量与光照切换、输入、Resize、资产 Fetch、资源释放、
  原生异步 Pipeline、上传取消与回滚、外部 Buffer 缺失诊断全部通过。
- `cmake --build --preset windows-vs2022-debug`：通过。
- `ctest --preset windows-vs2022-debug`：89/89 通过，包括 Vulkan、ABI、C/C++ Consumer 和
  Documentation。
- `cmake --build --preset windows-vs2022-static-debug`：通过。
- `ctest --preset windows-vs2022-static-debug`：74/74 通过。

## 未完成的远端验收

Linux、远端 SDK 和不可变 Release Candidate 验收继续由 S-37G 统一执行。本地工作区未推送，
因此本记录不把这些项目标记为通过。
