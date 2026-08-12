// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_LIGHTING_SHADOW_RESOURCES_H
#define GRANIT_LIGHTING_SHADOW_RESOURCES_H

#include "math/math.h"

#include <granit/renderer/buffer.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/texture.h>

namespace granit::lighting {

inline constexpr std::uint32_t shadow_binding_constants = 0;
inline constexpr std::uint32_t shadow_binding_texture = 1;
inline constexpr std::uint32_t shadow_binding_sampler = 2;

struct alignas(16) shadow_sampling_constants {
  math::matrix4 light_view_projection{};
  float depth_bias = 0.001F;
  float normal_bias = 0.0F;
  float texel_size[2]{};
};

static_assert(sizeof(shadow_sampling_constants) == 80);

/** 拥有 Group 3 阴影常量、比较 Sampler、布局和 Bind Group，不拥有阴影 Texture View。 */
class shadow_resources {
public:
  [[nodiscard]] granit_result initialize(granit_renderer renderer, granit_texture_view shadow_view,
                                         const shadow_sampling_constants& constants) noexcept;
  [[nodiscard]] granit_result update(const shadow_sampling_constants& constants) noexcept;
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
