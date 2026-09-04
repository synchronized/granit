// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "reference/material/pbr_reference.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace granit::material {
namespace {

float saturate(float value) noexcept { return std::clamp(value, 0.0F, 1.0F); }

pbr_float3 multiply_components(pbr_float3 left, pbr_float3 right) noexcept {
  return {left.x * right.x, left.y * right.y, left.z * right.z};
}

pbr_float3 mix(pbr_float3 left, pbr_float3 right, float amount) noexcept {
  return math::add(math::multiply(left, 1.0F - amount), math::multiply(right, amount));
}

} // namespace

pbr_float3 pbr_fresnel_schlick(float view_dot_half, pbr_float3 reflectance_at_normal) noexcept {
  const auto factor = std::pow(1.0F - saturate(view_dot_half), 5.0F);
  return {reflectance_at_normal.x + (1.0F - reflectance_at_normal.x) * factor,
          reflectance_at_normal.y + (1.0F - reflectance_at_normal.y) * factor,
          reflectance_at_normal.z + (1.0F - reflectance_at_normal.z) * factor};
}

float pbr_distribution_ggx(float normal_dot_half, float perceptual_roughness) noexcept {
  const auto roughness = std::max(saturate(perceptual_roughness), pbr_minimum_perceptual_roughness);
  const auto alpha = roughness * roughness;
  const auto alpha_squared = alpha * alpha;
  const auto cosine = saturate(normal_dot_half);
  const auto denominator = cosine * cosine * (alpha_squared - 1.0F) + 1.0F;
  return alpha_squared / (std::numbers::pi_v<float> * denominator * denominator);
}

float pbr_visibility_smith_correlated(float normal_dot_view, float normal_dot_light,
                                      float perceptual_roughness) noexcept {
  const auto roughness = std::max(saturate(perceptual_roughness), pbr_minimum_perceptual_roughness);
  const auto alpha = roughness * roughness;
  const auto alpha_squared = alpha * alpha;
  const auto view = saturate(normal_dot_view);
  const auto light = saturate(normal_dot_light);
  const auto lambda_view = light * std::sqrt(view * view * (1.0F - alpha_squared) + alpha_squared);
  const auto lambda_light =
      view * std::sqrt(light * light * (1.0F - alpha_squared) + alpha_squared);
  const auto denominator = lambda_view + lambda_light;
  return denominator > 0.0F ? 0.5F / denominator : 0.0F;
}

pbr_float3 evaluate_pbr_direct_light(const pbr_material_parameters& material,
                                     const pbr_direct_light_input& input) noexcept {
  const auto normal = math::normalize(input.normal);
  const auto view = math::normalize(input.view_direction);
  const auto light = math::normalize(input.light_direction);
  const auto half_vector = math::normalize(math::add(view, light));
  const auto normal_dot_view = saturate(math::dot(normal, view));
  const auto normal_dot_light = saturate(math::dot(normal, light));
  if (normal_dot_view <= 0.0F || normal_dot_light <= 0.0F)
    return {};

  const auto metallic = saturate(material.metallic);
  const auto base_color =
      pbr_float3{std::max(material.base_color.x, 0.0F), std::max(material.base_color.y, 0.0F),
                 std::max(material.base_color.z, 0.0F)};
  const auto reflectance =
      mix({pbr_dielectric_f0, pbr_dielectric_f0, pbr_dielectric_f0}, base_color, metallic);
  const auto fresnel = pbr_fresnel_schlick(saturate(math::dot(view, half_vector)), reflectance);
  const auto distribution =
      pbr_distribution_ggx(saturate(math::dot(normal, half_vector)), material.perceptual_roughness);
  const auto visibility = pbr_visibility_smith_correlated(normal_dot_view, normal_dot_light,
                                                          material.perceptual_roughness);
  const auto diffuse_scale = (1.0F - metallic) / std::numbers::pi_v<float>;
  const auto diffuse = pbr_float3{base_color.x * (1.0F - fresnel.x) * diffuse_scale,
                                  base_color.y * (1.0F - fresnel.y) * diffuse_scale,
                                  base_color.z * (1.0F - fresnel.z) * diffuse_scale};
  const auto specular = math::multiply(fresnel, distribution * visibility);
  return math::multiply(multiply_components(math::add(diffuse, specular), input.radiance),
                        normal_dot_light);
}

} // namespace granit::material
