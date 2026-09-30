<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# 2026-09-30 S-81 独立纹理变体上传验收

## 范围

本记录覆盖 Gneiss `UPSTREAM-044` 的 U44-01：以所选 Texture Asset 变体起点为基址上传完整
局部负载，不新增 Gneiss 的预算、VFS、调度或 RID 生命周期。

## 实现结果

- 新增 `granit_upload_batch_write_texture_asset_variant_mips` C API 和对应 C++ 包装；
- 新入口要求 Payload 长度等于完整变体长度，并按相对 `data_offset` 校验和上传；
- 旧 `granit_upload_batch_write_texture_asset_mips` 保留完整 Manifest Payload 基址语义；
- 复用摘要校验、Mip 范围检查、Batch 背压、失败清理和提交生命周期。

## 验证结果

- Windows Clang Debug 完整构建通过；
- 独立变体、旧完整 Payload、截断负载和损坏摘要测试通过，11 assertions；
- ABI exports、Renderer resources、Documentation links 通过；
- PR #125 的 Linux、Windows、Emscripten、浏览器和安装 Consumer 矩阵通过；
- 首次浏览器 Tutorial 02/03 和平台 smoke 失败属于 Dawn/WebGPU 运行时瞬时波动，重跑后通过，
  未发现本次 Texture Asset API 引入的回归。

## 边界

视距驱动 Mip 流送、虚拟纹理、预算管理和 Gneiss 资源事务仍由 Gneiss 负责，不属于 Granit
U44-01 的公共契约。
