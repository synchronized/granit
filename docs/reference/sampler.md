<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Sampler

Sampler 是独立于 Texture 的采样状态，可以由多个 Bind Group 复用。C API 使用 64 位句柄，
C++20 提供 move-only `granit::sampler`。

## 公共入口

- C：`<granit/renderer/sampler.h>`，使用 `granit_sampler_create` 和
  `granit_sampler_destroy`。
- C++20：`<granit/renderer/sampler.hpp>`，使用 move-only 的 `granit::sampler`。
- Sampler 是 Core Renderer 资源，目标为 `granit::granit`。

使用 `GRANIT_SAMPLER_DESC_INIT` 初始化 `granit_sampler_desc`，或在 C++ 中使用默认的
`granit::sampler_desc`。描述中的过滤、寻址、比较、各向异性和 LOD 字段会在创建时完整校验。

默认描述为 linear 过滤、linear mip、repeat 寻址、关闭对比和各向异性、LOD 范围为零。

支持全部 core compare operation。各向异性和 LOD bias 会按当前设备 feature 与 limit 严格验证，
不支持或超过上限时返回 `GRANIT_ERROR_UNSUPPORTED`，不会静默修改描述。
最大各向异性可通过 `granit_renderer_get_limits` 的 `max_sampler_anisotropy` 查询；调用者可以据此
选择 1×、2×、4×、8× 或 16× 等质量档，并应显示最终实际请求值。

当前不缓存相同描述，每次创建拥有独立公共和后端对象。Renderer 销毁会级联使 Sampler 句柄
失效。

Sampler 创建后不可更新。不同 Sampler 可以并发创建和使用；不要让同一个 Sampler 的销毁与
Bind Group 创建、命令录制或渲染使用并发执行。跨 Renderer 使用、重复销毁或无效句柄返回
`GRANIT_ERROR_INVALID_HANDLE`。
