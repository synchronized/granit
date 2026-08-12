// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_IBL_RESOURCES_H
#define GRANIT_LIGHTING_IBL_RESOURCES_H

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.h>

namespace granit::lighting {

inline constexpr std::uint32_t ibl_binding_constants = 3;
inline constexpr std::uint32_t ibl_binding_irradiance = 4;
inline constexpr std::uint32_t ibl_binding_prefiltered_environment = 5;
inline constexpr std::uint32_t ibl_binding_brdf_lut = 6;
inline constexpr std::uint32_t ibl_binding_sampler = 7;

struct alignas(16) ibl_sampling_constants {
  float rotation_cos = 1.0F;
  float rotation_sin = 0.0F;
  float intensity = 1.0F;
  float prefiltered_max_mip = 0.0F;
};

static_assert(sizeof(ibl_sampling_constants) == 16);

struct ibl_texture_views {
  granit_texture_view irradiance = GRANIT_NULL_HANDLE;
  granit_texture_view prefiltered_environment = GRANIT_NULL_HANDLE;
  granit_texture_view brdf_lut = GRANIT_NULL_HANDLE;
};

/**
 * 拥有 Group 3 IBL 常量、线性 Sampler、布局和 Bind Group，不拥有三个 Texture View。
 * binding 3～7 为阴影 binding 0～2 保留组合空间。
 */
class ibl_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer, ibl_texture_views views,
                                         const ibl_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result update(const ibl_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result reset() noexcept;
  [[nodiscard]] bool initialized() const noexcept { return group_.valid(); }
  [[nodiscard]] granit_bind_group_layout layout() const noexcept { return layout_.native_handle(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return group_.native_handle(); }

private:
  granit::buffer constants_;
  granit::sampler sampler_;
  granit::bind_group_layout layout_;
  granit::bind_group group_;
};

} // namespace granit::lighting

#endif
