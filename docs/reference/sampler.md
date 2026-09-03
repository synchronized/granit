<!-- SPDX-License-Identifier: MIT -->
<!-- Copyright (c) 2026 Granit contributors -->

# Sampler

Sampler 是独立于 Texture 的采样状态，可以由多个未来的资源绑定复用。C API 使用 64 位句柄，
C++20 提供 move-only `granit::sampler`。

默认描述为 linear 过滤、linear mip、repeat 寻址、关闭对比和各向异性、LOD 范围为零。

支持全部 core compare operation。各向异性和 LOD bias 会按当前设备 feature 与 limit 严格验证，
不支持或超过上限时返回 `GRANIT_ERROR_UNSUPPORTED`，不会静默修改描述。
最大各向异性可通过 `granit_renderer_get_limits` 的 `max_sampler_anisotropy` 查询；调用者可以据此
选择 1×、2×、4×、8× 或 16× 等质量档，并应显示最终实际请求值。

首期不缓存相同描述，每次创建拥有独立公共和 Vulkan 对象。Renderer 销毁会级联使 Sampler 句柄
失效。
