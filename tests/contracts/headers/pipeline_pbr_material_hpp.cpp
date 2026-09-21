// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.hpp>

static_assert((granit::pbr_texture::base_color | granit::pbr_texture::normal) !=
              granit::pbr_texture::none);
static_assert(requires(std::span<const granit::vertex_buffer_layout> layouts) {
  granit::validate_pbr_vertex_layout(layouts, granit::pbr_texture::normal);
});
