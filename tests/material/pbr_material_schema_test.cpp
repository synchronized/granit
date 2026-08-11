// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_material_schema.h"

#include <catch2/catch_all.hpp>

namespace {

granit::material::material_vertex_buffer_layout standard_layout() {
  using namespace granit::material;
  return {.stride = 48,
          .step_mode = GRANIT_VERTEX_STEP_MODE_VERTEX,
          .attributes = {{pbr_vertex_location_position, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0},
                         {pbr_vertex_location_normal, GRANIT_VERTEX_FORMAT_FLOAT32X3, 12},
                         {pbr_vertex_location_tangent, GRANIT_VERTEX_FORMAT_FLOAT32X4, 24},
                         {pbr_vertex_location_uv0, GRANIT_VERTEX_FORMAT_FLOAT32X2, 40}}};
}

} // namespace

TEST_CASE("PBR 无纹理顶点布局只要求位置和法线") {
  auto layout = standard_layout();
  layout.attributes.resize(2);
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1}, 0) ==
        granit::material::pbr_vertex_layout_error::none);
  layout.attributes.pop_back();
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1}, 0) ==
        granit::material::pbr_vertex_layout_error::missing_normal);
}

TEST_CASE("PBR 纹理布局要求 UV 且法线贴图额外要求切线") {
  auto layout = standard_layout();
  layout.attributes.erase(layout.attributes.begin() + 2);
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1},
                                                     granit::material::pbr_texture_base_color) ==
        granit::material::pbr_vertex_layout_error::none);
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1},
                                                     granit::material::pbr_texture_normal) ==
        granit::material::pbr_vertex_layout_error::missing_tangent);
  layout.attributes.pop_back();
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1},
                                                     granit::material::pbr_texture_emissive) ==
        granit::material::pbr_vertex_layout_error::missing_uv0);
}

TEST_CASE("PBR 顶点布局拒绝未知纹理 feature 位") {
  const auto layout = standard_layout();
  CHECK(granit::material::validate_pbr_vertex_layout(std::span{&layout, 1}, UINT32_C(1) << 31) ==
        granit::material::pbr_vertex_layout_error::invalid_texture_flags);
}
