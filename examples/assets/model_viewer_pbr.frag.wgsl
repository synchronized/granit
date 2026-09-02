// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

const pi = 3.14159265358979323846;

struct FrameConstants {
  view_projection: mat4x4<f32>,
  camera_position: vec4f,
  direction_to_light: vec4f,
  light_radiance: vec4f,
}

struct MaterialConstants {
  base_color: vec4f,
  metallic: f32,
  perceptual_roughness: f32,
  normal_scale: f32,
  occlusion_strength: f32,
  emissive: vec3f,
  debug_display: u32,
}

struct IblConstants {
  environment_rotation_cos: f32,
  environment_rotation_sin: f32,
  environment_intensity: f32,
  prefiltered_environment_max_mip: f32,
}

struct FragmentInput {
  @location(0) world_position: vec3f,
  @location(1) world_normal: vec3f,
  @location(2) world_tangent: vec4f,
  @location(3) texture_coordinate: vec2f,
  @location(4) vertex_normal: vec3f,
  @location(5) vertex_tangent: vec3f,
}

@group(0) @binding(0) var<uniform> frame: FrameConstants;
@group(1) @binding(0) var<uniform> material: MaterialConstants;
@group(1) @binding(1) var base_color_texture: texture_2d<f32>;
@group(1) @binding(2) var metallic_roughness_texture: texture_2d<f32>;
@group(1) @binding(3) var normal_texture: texture_2d<f32>;
@group(1) @binding(4) var occlusion_texture: texture_2d<f32>;
@group(1) @binding(5) var emissive_texture: texture_2d<f32>;
@group(1) @binding(6) var pbr_sampler: sampler;
@group(3) @binding(3) var<uniform> ibl: IblConstants;
@group(3) @binding(4) var irradiance_texture: texture_cube<f32>;
@group(3) @binding(5) var prefiltered_environment_texture: texture_cube<f32>;
@group(3) @binding(6) var brdf_lut_texture: texture_2d<f32>;
@group(3) @binding(7) var environment_sampler: sampler;

fn distribution_ggx(normal: vec3f, halfway: vec3f, roughness: f32) -> f32 {
  let alpha = roughness * roughness;
  let alpha_squared = alpha * alpha;
  let normal_dot_halfway = max(dot(normal, halfway), 0.0);
  let denominator = normal_dot_halfway * normal_dot_halfway * (alpha_squared - 1.0) + 1.0;
  return alpha_squared / max(pi * denominator * denominator, 0.0001);
}

fn geometry_schlick_ggx(normal_dot_direction: f32, roughness: f32) -> f32 {
  let k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
  return normal_dot_direction / max(normal_dot_direction * (1.0 - k) + k, 0.0001);
}

fn fresnel_schlick(cosine: f32, reflectance: vec3f) -> vec3f {
  return reflectance + (vec3f(1.0) - reflectance) * pow(1.0 - cosine, 5.0);
}

fn fresnel_schlick_roughness(cosine: f32, reflectance: vec3f,
                             roughness: f32) -> vec3f {
  return reflectance + (max(vec3f(1.0 - roughness), reflectance) - reflectance) *
                       pow(1.0 - cosine, 5.0);
}

fn rotate_environment(direction: vec3f) -> vec3f {
  return vec3f(ibl.environment_rotation_cos * direction.x +
                   ibl.environment_rotation_sin * direction.z,
               direction.y,
               -ibl.environment_rotation_sin * direction.x +
                   ibl.environment_rotation_cos * direction.z);
}

@fragment
fn fragment_main(input: FragmentInput) -> @location(0) vec4f {
  let sampled_base_color = textureSample(base_color_texture, pbr_sampler,
                                         input.texture_coordinate);
  let base_color = material.base_color * sampled_base_color;
  let metallic_roughness = textureSample(metallic_roughness_texture, pbr_sampler,
                                         input.texture_coordinate);
  let metallic = clamp(material.metallic * metallic_roughness.b, 0.0, 1.0);
  let roughness = clamp(material.perceptual_roughness * metallic_roughness.g, 0.045, 1.0);

  let tangent = normalize(input.world_tangent.xyz);
  let geometric_normal = normalize(input.world_normal);
  let bitangent = normalize(cross(geometric_normal, tangent)) * input.world_tangent.w;
  let sampled_normal = textureSample(normal_texture, pbr_sampler,
                                     input.texture_coordinate).xyz * 2.0 - vec3f(1.0);
  let scaled_normal = vec3f(sampled_normal.xy * material.normal_scale, sampled_normal.z);
  let normal = normalize(mat3x3<f32>(tangent, bitangent, geometric_normal) * scaled_normal);

  let view_direction = normalize(frame.camera_position.xyz - input.world_position);
  let light_direction = normalize(frame.direction_to_light.xyz);
  let halfway = normalize(view_direction + light_direction);
  let normal_dot_light = max(dot(normal, light_direction), 0.0);
  let normal_dot_view = max(dot(normal, view_direction), 0.0);
  let reflectance = mix(vec3f(0.04), base_color.rgb, metallic);
  let fresnel = fresnel_schlick(max(dot(halfway, view_direction), 0.0), reflectance);
  let distribution = distribution_ggx(normal, halfway, roughness);
  let geometry = geometry_schlick_ggx(normal_dot_light, roughness) *
                 geometry_schlick_ggx(normal_dot_view, roughness);
  let specular = distribution * geometry * fresnel / max(4.0 * normal_dot_light *
                                                         normal_dot_view, 0.0001);
  let diffuse = (vec3f(1.0) - fresnel) * (1.0 - metallic) * base_color.rgb / pi;
  let occlusion_sample = textureSample(occlusion_texture, pbr_sampler,
                                       input.texture_coordinate).r;
  let occlusion = mix(1.0, occlusion_sample, material.occlusion_strength);
  let emissive = material.emissive * textureSample(emissive_texture, pbr_sampler,
                                                   input.texture_coordinate).rgb;
  if (material.debug_display == 1u) {
    return vec4f(base_color.rgb, base_color.a);
  }
  if (material.debug_display == 2u) {
    return vec4f(normal * 0.5 + vec3f(0.5), 1.0);
  }
  if (material.debug_display == 3u) {
    return vec4f(vec3f(metallic), 1.0);
  }
  if (material.debug_display == 4u) {
    return vec4f(vec3f(roughness), 1.0);
  }
  if (material.debug_display == 5u) {
    return vec4f(geometric_normal * 0.5 + vec3f(0.5), 1.0);
  }
  if (material.debug_display == 6u) {
    return vec4f(sampled_normal * 0.5 + vec3f(0.5), 1.0);
  }
  if (material.debug_display == 7u) {
    return vec4f(normalize(input.vertex_normal) * 0.5 + vec3f(0.5), 1.0);
  }
  if (material.debug_display == 8u) {
    return vec4f(normalize(input.vertex_tangent) * 0.5 + vec3f(0.5), 1.0);
  }
  let environment_fresnel = fresnel_schlick_roughness(normal_dot_view, reflectance,
                                                       roughness);
  let environment_diffuse = textureSample(irradiance_texture, environment_sampler,
                                          rotate_environment(normal)).rgb;
  let reflection = reflect(-view_direction, normal);
  let environment_specular = textureSampleLevel(
      prefiltered_environment_texture, environment_sampler,
      rotate_environment(reflection), roughness * ibl.prefiltered_environment_max_mip).rgb;
  let brdf = textureSample(brdf_lut_texture, environment_sampler,
                           vec2f(normal_dot_view, roughness)).rg;
  let ambient = (((vec3f(1.0) - environment_fresnel) * (1.0 - metallic) *
                  base_color.rgb * environment_diffuse / pi) +
                 environment_specular * (environment_fresnel * brdf.x + brdf.y)) *
                ibl.environment_intensity * occlusion;
  let color = ambient + (diffuse + specular) * frame.light_radiance.rgb * normal_dot_light +
              emissive;
  return vec4f(color, base_color.a);
}
