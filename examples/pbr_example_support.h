// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PBR_EXAMPLE_SUPPORT_H
#define GRANIT_EXAMPLES_PBR_EXAMPLE_SUPPORT_H

#include "lighting/shadow_ibl_resources.h"
#include "material/material_gpu_instance.h"
#include "material/material_package.h"
#include "material/material_template_gpu.h"
#include "material/pbr_default_resources.h"

#include <granit/core/result.hpp>

#include <span>
#include <string_view>

namespace granit::examples {

class pbr_lighting_resources {
public:
  [[nodiscard]] result initialize(granit_renderer renderer);
  [[nodiscard]] result update_lights(const lighting::packed_view_lights& lights) {
    return from_native(resources_.update_lights(lights));
  }
  [[nodiscard]] result reset();
  [[nodiscard]] granit_bind_group_layout layout() const noexcept { return resources_.layout(); }
  [[nodiscard]] granit_bind_group group() const noexcept { return resources_.group(); }
  [[nodiscard]] granit_texture_view shadow_view() const noexcept {
    return shadow_view_.native_handle();
  }

private:
  texture shadow_texture_;
  texture_view shadow_view_;
  texture irradiance_texture_;
  texture prefiltered_texture_;
  texture brdf_lut_texture_;
  texture_view irradiance_view_;
  texture_view prefiltered_view_;
  texture_view brdf_lut_view_;
  lighting::shadow_ibl_resources resources_;
};

[[nodiscard]] bool build_pbr_package(material::material_package& package,
                                     std::span<const std::uint32_t> vertex_shader,
                                     std::string_view vertex_wgsl,
                                     std::span<const std::uint32_t> fragment_shader,
                                     std::string_view fragment_wgsl);

[[nodiscard]] result initialize_pbr_instance(granit_renderer renderer,
                                             material::material_template_gpu& material_template,
                                             const material::material_package& package,
                                             material::pbr_default_resources& defaults,
                                             material::material_gpu_instance& instance);

} // namespace granit::examples

#endif
