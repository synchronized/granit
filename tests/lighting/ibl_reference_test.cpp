// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/ibl_reference.h"

#include <catch2/catch_all.hpp>

#include <numbers>

TEST_CASE("IBL粗糙度映射覆盖预过滤环境完整mip范围") {
  CHECK(granit::lighting::ibl_prefilter_mip(0.0F, 5.0F) == 0.0F);
  CHECK(granit::lighting::ibl_prefilter_mip(0.5F, 5.0F) == 2.5F);
  CHECK(granit::lighting::ibl_prefilter_mip(2.0F, 5.0F) == 5.0F);
  CHECK(granit::lighting::ibl_prefilter_mip(0.5F, -1.0F) == 0.0F);
}

TEST_CASE("环境旋转绕世界Y轴转换查询方向") {
  const auto rotated = granit::lighting::rotate_environment_direction(
      {0.0F, 0.0F, 1.0F}, std::numbers::pi_v<float> * 0.5F);
  CHECK(rotated.x == Catch::Approx(1.0F));
  CHECK(rotated.y == Catch::Approx(0.0F));
  CHECK(rotated.z == Catch::Approx(0.0F).margin(0.000001F));
}

TEST_CASE("Split-sum IBL区分非金属漫反射和金属镜面反射") {
  granit::lighting::ibl_reference_input input{};
  input.irradiance = {1.0F, 1.0F, 1.0F};
  input.prefiltered_radiance = {2.0F, 2.0F, 2.0F};
  input.brdf_lut = {0.5F, 0.1F};

  granit::material::pbr_material_parameters dielectric{{0.8F, 0.4F, 0.2F}, 0.0F, 0.5F};
  const auto dielectric_result = granit::lighting::evaluate_pbr_ibl(dielectric, input);
  CHECK(dielectric_result.x > dielectric_result.y);
  CHECK(dielectric_result.y > dielectric_result.z);

  auto metal = dielectric;
  metal.metallic = 1.0F;
  const auto metal_result = granit::lighting::evaluate_pbr_ibl(metal, input);
  CHECK(metal_result.x == Catch::Approx(1.0F));
  CHECK(metal_result.y == Catch::Approx(0.6F));
  CHECK(metal_result.z == Catch::Approx(0.4F));
}

TEST_CASE("缺省环境和完全遮蔽产生零间接光") {
  const granit::material::pbr_material_parameters material{};
  CHECK(granit::lighting::evaluate_pbr_ibl(material, {}) == granit::material::pbr_float3{});

  granit::lighting::ibl_reference_input input{};
  input.irradiance = {1.0F, 1.0F, 1.0F};
  input.prefiltered_radiance = {1.0F, 1.0F, 1.0F};
  input.brdf_lut = {1.0F, 1.0F};
  input.ambient_occlusion = 0.0F;
  CHECK(granit::lighting::evaluate_pbr_ibl(material, input) == granit::material::pbr_float3{});
}
