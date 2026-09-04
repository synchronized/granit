// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "reference/lighting/ibl_reference.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace granit::lighting {
namespace {

float saturate(float value) noexcept { return std::clamp(value, 0.0F, 1.0F); }

material::pbr_float3 multiply_components(material::pbr_float3 left,
                                         material::pbr_float3 right) noexcept {
  return {left.x * right.x, left.y * right.y, left.z * right.z};
}

material::pbr_float3 mix(material::pbr_float3 left, material::pbr_float3 right,
                         float amount) noexcept {
  return math::add(math::multiply(left, 1.0F - amount), math::multiply(right, amount));
}

material::pbr_float3 fresnel_schlick_roughness(float normal_dot_view,
                                               material::pbr_float3 reflectance,
                                               float roughness) noexcept {
  const auto grazing = 1.0F - roughness;
  const auto factor = std::pow(1.0F - normal_dot_view, 5.0F);
  return {reflectance.x + (std::max(grazing, reflectance.x) - reflectance.x) * factor,
          reflectance.y + (std::max(grazing, reflectance.y) - reflectance.y) * factor,
          reflectance.z + (std::max(grazing, reflectance.z) - reflectance.z) * factor};
}

} // namespace

float ibl_prefilter_mip(float perceptual_roughness, float max_mip_level) noexcept {
  return saturate(perceptual_roughness) * std::max(max_mip_level, 0.0F);
}

material::pbr_float3 rotate_environment_direction(material::pbr_float3 direction,
                                                  float rotation_radians) noexcept {
  const auto normalized = math::normalize(direction);
  if (!std::isfinite(rotation_radians) || normalized == material::pbr_float3{})
    return {};

  const auto cosine = std::cos(rotation_radians);
  const auto sine = std::sin(rotation_radians);
  return {cosine * normalized.x + sine * normalized.z, normalized.y,
          -sine * normalized.x + cosine * normalized.z};
}

material::pbr_float3 evaluate_pbr_ibl(const material::pbr_material_parameters& material_parameters,
                                      const ibl_reference_input& input) noexcept {
  const auto normal = math::normalize(input.normal);
  const auto view = math::normalize(input.view_direction);
  const auto normal_dot_view = saturate(math::dot(normal, view));
  if (normal_dot_view <= 0.0F)
    return {};

  const auto metallic = saturate(material_parameters.metallic);
  const auto roughness = saturate(material_parameters.perceptual_roughness);
  const auto base_color = material::pbr_float3{std::max(material_parameters.base_color.x, 0.0F),
                                               std::max(material_parameters.base_color.y, 0.0F),
                                               std::max(material_parameters.base_color.z, 0.0F)};
  const auto reflectance =
      mix({material::pbr_dielectric_f0, material::pbr_dielectric_f0, material::pbr_dielectric_f0},
          base_color, metallic);
  const auto fresnel = fresnel_schlick_roughness(normal_dot_view, reflectance, roughness);
  const auto diffuse_weight = material::pbr_float3{(1.0F - fresnel.x) * (1.0F - metallic),
                                                   (1.0F - fresnel.y) * (1.0F - metallic),
                                                   (1.0F - fresnel.z) * (1.0F - metallic)};
  const auto diffuse = math::multiply(
      multiply_components(multiply_components(diffuse_weight, base_color), input.irradiance),
      1.0F / std::numbers::pi_v<float>);
  const auto specular_factor =
      material::pbr_float3{fresnel.x * input.brdf_lut.x + input.brdf_lut.y,
                           fresnel.y * input.brdf_lut.x + input.brdf_lut.y,
                           fresnel.z * input.brdf_lut.x + input.brdf_lut.y};
  const auto specular = multiply_components(input.prefiltered_radiance, specular_factor);
  const auto scale =
      std::max(input.environment_intensity, 0.0F) * saturate(input.ambient_occlusion);
  return math::multiply(math::add(diffuse, specular), scale);
}

} // namespace granit::lighting
