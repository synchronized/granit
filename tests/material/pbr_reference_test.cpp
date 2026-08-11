// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_reference.h"

#include <catch2/catch_all.hpp>

namespace {

void check_color(granit::material::pbr_float3 actual, granit::material::pbr_float3 expected) {
  CHECK(actual.x == Catch::Approx(expected.x).margin(0.00001F));
  CHECK(actual.y == Catch::Approx(expected.y).margin(0.00001F));
  CHECK(actual.z == Catch::Approx(expected.z).margin(0.00001F));
}

} // namespace

TEST_CASE("PBR 参考实现匹配正入射介电材质固定向量") {
  const granit::material::pbr_material_parameters material{};
  const granit::material::pbr_direct_light_input light{};
  check_color(granit::material::evaluate_pbr_direct_light(material, light),
              {0.30876058F, 0.30876058F, 0.30876058F});
}

TEST_CASE("PBR 参考实现匹配正入射金属材质固定向量") {
  const granit::material::pbr_material_parameters material{
      .base_color = {1.0F, 1.0F, 1.0F}, .metallic = 1.0F, .perceptual_roughness = 1.0F};
  const granit::material::pbr_direct_light_input light{};
  check_color(granit::material::evaluate_pbr_direct_light(material, light),
              {0.07957747F, 0.07957747F, 0.07957747F});
}

TEST_CASE("PBR 参考实现匹配彩色半金属固定向量") {
  const granit::material::pbr_material_parameters material{
      .base_color = {0.8F, 0.2F, 0.1F}, .metallic = 0.5F, .perceptual_roughness = 0.5F};
  const granit::material::pbr_direct_light_input light{.radiance = {2.0F, 1.0F, 0.5F}};
  check_color(granit::material::evaluate_pbr_direct_light(material, light),
              {1.21721578F, 0.18080002F, 0.05196443F});
}

TEST_CASE("PBR 参考实现处理背光和参数边界") {
  const granit::material::pbr_material_parameters material{
      .base_color = {-1.0F, 1.0F, 1.0F}, .metallic = 2.0F, .perceptual_roughness = -1.0F};
  const granit::material::pbr_direct_light_input back_light{.light_direction = {0.0F, 0.0F, -1.0F}};
  CHECK(granit::material::evaluate_pbr_direct_light(material, back_light) ==
        granit::material::pbr_float3{});
  CHECK(granit::material::pbr_distribution_ggx(1.0F, -1.0F) ==
        Catch::Approx(granit::material::pbr_distribution_ggx(
            1.0F, granit::material::pbr_minimum_perceptual_roughness)));
}
