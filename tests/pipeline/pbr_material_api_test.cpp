// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/pbr_material.h>

#include <array>
#include <catch2/catch_all.hpp>

namespace {

constexpr std::array standard_attributes{
    granit_vertex_attribute{GRANIT_PBR_VERTEX_LOCATION_POSITION, GRANIT_VERTEX_FORMAT_FLOAT32X3, 0,
                            0},
    granit_vertex_attribute{GRANIT_PBR_VERTEX_LOCATION_NORMAL, GRANIT_VERTEX_FORMAT_FLOAT32X3, 12,
                            0},
    granit_vertex_attribute{GRANIT_PBR_VERTEX_LOCATION_TANGENT, GRANIT_VERTEX_FORMAT_FLOAT32X4, 24,
                            0},
    granit_vertex_attribute{GRANIT_PBR_VERTEX_LOCATION_UV0, GRANIT_VERTEX_FORMAT_FLOAT32X2, 40, 0},
};

} // namespace

TEST_CASE("公共 PBR Schema 验证标准顶点布局") {
  const granit_vertex_buffer_layout layout{48, GRANIT_VERTEX_STEP_MODE_VERTEX,
                                           static_cast<std::uint32_t>(standard_attributes.size()),
                                           0, standard_attributes.data()};
  CHECK(granit_pbr_validate_vertex_layout(&layout, 1, GRANIT_PBR_TEXTURE_ALL) ==
        GRANIT_PBR_VERTEX_LAYOUT_VALID);
  CHECK(granit_pbr_validate_vertex_layout(&layout, 1, UINT32_C(1) << 31) ==
        GRANIT_PBR_VERTEX_LAYOUT_INVALID_TEXTURE_FLAGS);
}

TEST_CASE("公共 PBR Schema 按纹理要求 UV 和切线") {
  std::array attributes = standard_attributes;
  const granit_vertex_buffer_layout without_tangent{48, GRANIT_VERTEX_STEP_MODE_VERTEX, 3, 0,
                                                    attributes.data()};
  attributes[2] = attributes[3];
  CHECK(granit_pbr_validate_vertex_layout(&without_tangent, 1, GRANIT_PBR_TEXTURE_BASE_COLOR) ==
        GRANIT_PBR_VERTEX_LAYOUT_VALID);
  CHECK(granit_pbr_validate_vertex_layout(&without_tangent, 1, GRANIT_PBR_TEXTURE_NORMAL) ==
        GRANIT_PBR_VERTEX_LAYOUT_MISSING_TANGENT);
  CHECK(granit_pbr_validate_vertex_layout(nullptr, 1, 0) ==
        GRANIT_PBR_VERTEX_LAYOUT_INVALID_ARGUMENT);
}
