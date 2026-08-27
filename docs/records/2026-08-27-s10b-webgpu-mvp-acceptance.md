<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# S-10B 桌面 WebGPU MVP 验收

## 结论

S-10B 已完成。Granit 自有 WebGPU 插件在 Windows D3D12 和 Linux Vulkan 上均通过锁定 Dawn
静态库构建、插件加载、资源生命周期、命令提交和确定性离屏像素回归。Dawn 头文件、链接依赖和
原生对象仍保持在内部插件边界内，未进入 Granit 公共 API 或安装 Consumer。

## 验收范围

- 插件 Instance、Adapter、Device、Queue 与能力快照。
- Buffer 写入、映射回读及范围和 Usage 校验。
- 二维 RGBA8 Texture、Texture View、Sampler、Bind Group 与 Pipeline。
- Recorder、Render Pass、Draw、Buffer/Texture Copy 与一次性 Queue Submit。
- 64×64 离屏绿色三角形及背景、中心像素和量化容差校验。
- 跨实例、重复销毁、依赖销毁顺序、级联清理和插件卸载。
- fallback adapter 优先选择及不可用时的普通 adapter 自动回退。

## 验证结果

- 本地 Windows Clang：Mock 插件 ABI、资源、生命周期与 fallback 回退测试通过。
- GitHub Actions Windows 2022：Dawn D3D12 静态产物、符号检查和真实插件 smoke test 通过。
- GitHub Actions Ubuntu：Dawn Vulkan 静态产物、符号检查、Lavapipe 和真实插件 smoke test 通过。
- 双平台产物均完成打包和短期 Artifact 上传；本轮未发布长期 SDK 预发布包。

最终验证运行：[Dawn Dependency Packages #33030619671](https://github.com/synchronized/granit/actions/runs/33030619671)。

## 与计划的差异

CI 原先将 fallback adapter 作为不可回退的强制选择，但 Dawn 在两类 runner 上均可能不暴露该分类。
最终实现改为优先请求 fallback，失败后记录警告并重试普通 adapter；Linux 仍固定 Lavapipe 驱动，
因此无 GPU runner 的验证保持可复现。

## 后续工作

S-10C 将独立确定 WGSL 权威输入、错误诊断、反射和缓存契约；S-10B 不扩展公共 SPIR-V API，也不
提前引入浏览器 Surface 或 Emscripten 主循环。
