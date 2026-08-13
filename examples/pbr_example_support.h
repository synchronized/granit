// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PBR_EXAMPLE_SUPPORT_H
#define GRANIT_EXAMPLES_PBR_EXAMPLE_SUPPORT_H

#include "material/material_gpu_instance.h"
#include "material/material_package.h"
#include "material/material_template_gpu.h"
#include "material/pbr_default_resources.h"

#include <granit/core/result.hpp>

#include <span>

namespace granit::examples {

[[nodiscard]] bool build_pbr_package(material::material_package& package,
                                     std::span<const std::uint32_t> vertex_shader,
                                     std::span<const std::uint32_t> fragment_shader);

[[nodiscard]] result initialize_pbr_instance(granit_renderer renderer,
                                             material::material_template_gpu& material_template,
                                             const material::material_package& package,
                                             material::pbr_default_resources& defaults,
                                             material::material_gpu_instance& instance);

} // namespace granit::examples

#endif
