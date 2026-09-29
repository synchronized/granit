// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

static const float PI = 3.14159265358979323846;

struct vertex_input {
  [[vk::location(0)]] float3 position : POSITION;
  [[vk::location(1)]] float3 normal : NORMAL;
  [[vk::location(2)]] float4 tangent : TANGENT;
  [[vk::location(3)]] float2 texture_coordinate : TEXCOORD0;
#if GRANIT_PBR_HAS_UV1
  [[vk::location(4)]] float2 texture_coordinate_1 : TEXCOORD1;
#endif
#if GRANIT_PBR_HAS_VERTEX_COLOR
  [[vk::location(5)]] float4 color : COLOR0;
#endif
};

struct vertex_output {
  float4 position : SV_Position;
  float3 world_position : TEXCOORD0;
  float3 world_normal : TEXCOORD1;
  float4 world_tangent : TEXCOORD2;
  float2 texture_coordinate : TEXCOORD3;
  float3 vertex_normal : TEXCOORD4;
  float3 vertex_tangent : TEXCOORD5;
#if GRANIT_PBR_HAS_UV1
  float2 texture_coordinate_1 : TEXCOORD6;
#endif
#if GRANIT_PBR_HAS_VERTEX_COLOR
  float4 color : TEXCOORD7;
#endif
};

[[vk::binding(0, 0)]] cbuffer FrameConstants {
  column_major float4x4 view_projection;
  float4 camera_position;
  float4 direction_to_light;
  float4 light_radiance;
  uint4 render_options;
};

[[vk::binding(0, 1)]] cbuffer MaterialConstants {
  float4 base_color;
  float metallic;
  float perceptual_roughness;
  float normal_scale;
  float occlusion_strength;
  float3 emissive;
  uint debug_display;
  float alpha_cutoff;
  uint uv1_mask;
  uint2 material_reserved;
};

[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(2, 1)]] Texture2D<float4> metallic_roughness_texture;
[[vk::binding(3, 1)]] Texture2D<float4> normal_texture;
[[vk::binding(4, 1)]] Texture2D<float4> occlusion_texture;
[[vk::binding(5, 1)]] Texture2D<float4> emissive_texture;
[[vk::binding(6, 1)]] SamplerState base_color_sampler;
[[vk::binding(7, 1)]] SamplerState metallic_roughness_sampler;
[[vk::binding(8, 1)]] SamplerState normal_sampler;
[[vk::binding(9, 1)]] SamplerState occlusion_sampler;
[[vk::binding(10, 1)]] SamplerState emissive_sampler;

[[vk::binding(0, 2)]] cbuffer ObjectConstants {
  column_major float4x4 model;
  column_major float4x4 normal_matrix;
  uint object_id;
  float transform_handedness;
  uint2 object_reserved;
};

[[vk::binding(3, 3)]] cbuffer IblConstants {
  float environment_rotation_cos;
  float environment_rotation_sin;
  float environment_intensity;
  float prefiltered_environment_max_mip;
};

[[vk::binding(4, 3)]] TextureCube<float4> irradiance_texture;
[[vk::binding(5, 3)]] TextureCube<float4> prefiltered_environment_texture;
[[vk::binding(6, 3)]] Texture2D<float4> brdf_lut_texture;
[[vk::binding(7, 3)]] SamplerState environment_sampler;

vertex_output vertex_main(vertex_input input) {
  vertex_output output;
  const float4 resolved_world_position = mul(model, float4(input.position, 1.0));
  output.position = mul(view_projection, resolved_world_position);
  output.world_position = resolved_world_position.xyz;
  const float3 world_normal = normalize(mul(normal_matrix, float4(input.normal, 0.0)).xyz);
  const float3 transformed_tangent = mul(model, float4(input.tangent.xyz, 0.0)).xyz;
  const float3 world_tangent =
      normalize(transformed_tangent - world_normal * dot(world_normal, transformed_tangent));
  output.world_normal = world_normal;
  output.world_tangent = float4(world_tangent, input.tangent.w * transform_handedness);
  output.texture_coordinate = input.texture_coordinate;
#if GRANIT_PBR_HAS_UV1
  output.texture_coordinate_1 = input.texture_coordinate_1;
#endif
#if GRANIT_PBR_HAS_VERTEX_COLOR
  output.color = input.color;
#endif
  output.vertex_normal = input.normal;
  output.vertex_tangent = input.tangent.xyz;
  return output;
}

float distribution_ggx(float3 normal, float3 halfway, float roughness) {
  const float alpha = roughness * roughness;
  const float alpha_squared = alpha * alpha;
  const float normal_dot_halfway = max(dot(normal, halfway), 0.0);
  const float denominator = normal_dot_halfway * normal_dot_halfway * (alpha_squared - 1.0) + 1.0;
  return alpha_squared / max(PI * denominator * denominator, 0.0001);
}

float geometry_schlick_ggx(float normal_dot_direction, float roughness) {
  const float k = (roughness + 1.0) * (roughness + 1.0) * 0.125;
  return normal_dot_direction / max(normal_dot_direction * (1.0 - k) + k, 0.0001);
}

float3 fresnel_schlick(float cosine, float3 reflectance) {
  return reflectance + (1.0 - reflectance) * pow(1.0 - cosine, 5.0);
}

float3 fresnel_schlick_roughness(float cosine, float3 reflectance, float roughness) {
  return reflectance +
         (max((1.0 - roughness).xxx, reflectance) - reflectance) * pow(1.0 - cosine, 5.0);
}

float3 rotate_environment(float3 direction) {
  return float3(environment_rotation_cos * direction.x + environment_rotation_sin * direction.z,
                direction.y,
                -environment_rotation_sin * direction.x + environment_rotation_cos * direction.z);
}

float4 encode_output(float3 color, float alpha) {
#if GRANIT_PBR_ALPHA_BLEND
  return float4(color * alpha, alpha);
#else
  return float4(color, alpha);
#endif
}

float4 fragment_main(vertex_output input, bool is_front_face : SV_IsFrontFace) : SV_Target0 {
  float2 base_color_uv = input.texture_coordinate;
  float2 metallic_roughness_uv = input.texture_coordinate;
  float2 normal_uv = input.texture_coordinate;
  float2 occlusion_uv = input.texture_coordinate;
  float2 emissive_uv = input.texture_coordinate;
#if GRANIT_PBR_HAS_UV1
  base_color_uv = (uv1_mask & 1) != 0 ? input.texture_coordinate_1 : base_color_uv;
  metallic_roughness_uv = (uv1_mask & 2) != 0 ? input.texture_coordinate_1 : metallic_roughness_uv;
  normal_uv = (uv1_mask & 4) != 0 ? input.texture_coordinate_1 : normal_uv;
  occlusion_uv = (uv1_mask & 8) != 0 ? input.texture_coordinate_1 : occlusion_uv;
  emissive_uv = (uv1_mask & 16) != 0 ? input.texture_coordinate_1 : emissive_uv;
#endif
  const float4 sampled_base_color =
      base_color_texture.Sample(base_color_sampler, base_color_uv);
  float4 resolved_base_color = base_color * sampled_base_color;
#if GRANIT_PBR_HAS_VERTEX_COLOR
  resolved_base_color *= input.color;
#endif
  const float4 sampled_metallic_roughness =
      metallic_roughness_texture.Sample(metallic_roughness_sampler, metallic_roughness_uv);
  const float resolved_metallic = saturate(metallic * sampled_metallic_roughness.b);
  const float sampled_roughness =
      clamp(perceptual_roughness * sampled_metallic_roughness.g, 0.045, 1.0);

  const float3 geometric_normal = normalize(input.world_normal) * (is_front_face ? 1.0 : -1.0);
  const float3 tangent = normalize(input.world_tangent.xyz -
                                   geometric_normal * dot(geometric_normal,
                                                          input.world_tangent.xyz));
  const float3 bitangent = normalize(cross(geometric_normal, tangent)) * input.world_tangent.w;
  const float3 sampled_normal =
      normal_texture.Sample(normal_sampler, normal_uv).xyz * 2.0 - 1.0;
  const float3 scaled_normal = float3(sampled_normal.xy * normal_scale, sampled_normal.z);
  const float3 normal = normalize(tangent * scaled_normal.x + bitangent * scaled_normal.y +
                                  geometric_normal * scaled_normal.z);
  float roughness = sampled_roughness;
  if (render_options.x != 0) {
    const float3 normal_dx = ddx(normal);
    const float3 normal_dy = ddy(normal);
    const float normal_variance = max(dot(normal_dx, normal_dx), dot(normal_dy, normal_dy));
    roughness =
        clamp(sqrt(sampled_roughness * sampled_roughness + min(normal_variance, 0.2)), 0.045, 1.0);
  }

  const float3 view_direction = normalize(camera_position.xyz - input.world_position);
  const float3 light_direction = normalize(direction_to_light.xyz);
  const float3 halfway = normalize(view_direction + light_direction);
  const float normal_dot_light = max(dot(normal, light_direction), 0.0);
  const float normal_dot_view = max(dot(normal, view_direction), 0.0);
  const float3 reflectance = lerp(0.04.xxx, resolved_base_color.rgb, resolved_metallic);
  const float3 fresnel = fresnel_schlick(max(dot(halfway, view_direction), 0.0), reflectance);
  const float distribution = distribution_ggx(normal, halfway, roughness);
  const float geometry = geometry_schlick_ggx(normal_dot_light, roughness) *
                         geometry_schlick_ggx(normal_dot_view, roughness);
  const float3 specular =
      distribution * geometry * fresnel / max(4.0 * normal_dot_light * normal_dot_view, 0.0001);
  const float3 diffuse = (1.0 - fresnel) * (1.0 - resolved_metallic) * resolved_base_color.rgb / PI;
  const float occlusion_sample = occlusion_texture.Sample(occlusion_sampler, occlusion_uv).r;
  const float occlusion = lerp(1.0, occlusion_sample, occlusion_strength);
  const float3 resolved_emissive =
      emissive * emissive_texture.Sample(emissive_sampler, emissive_uv).rgb;

  const float3 environment_fresnel =
      fresnel_schlick_roughness(normal_dot_view, reflectance, roughness);
  const float3 environment_diffuse =
      irradiance_texture.Sample(environment_sampler, rotate_environment(normal)).rgb;
  const float3 reflection = reflect(-view_direction, normal);
  const float3 environment_specular =
      prefiltered_environment_texture
          .SampleLevel(environment_sampler, rotate_environment(reflection),
                       roughness * prefiltered_environment_max_mip)
          .rgb;
  const float2 brdf =
      brdf_lut_texture.Sample(environment_sampler, float2(normal_dot_view, roughness)).rg;
  const float3 ambient = (((1.0 - environment_fresnel) * (1.0 - resolved_metallic) *
                               resolved_base_color.rgb * environment_diffuse / PI +
                           environment_specular * (environment_fresnel * brdf.x + brdf.y)) *
                          environment_intensity * occlusion);
  const float3 color =
      ambient + (diffuse + specular) * light_radiance.rgb * normal_dot_light + resolved_emissive;
#if GRANIT_PBR_ALPHA_MASK
  clip(resolved_base_color.a - alpha_cutoff);
#endif
  if (debug_display == 1)
    return encode_output(resolved_base_color.rgb, resolved_base_color.a);
  if (debug_display == 2)
    return encode_output(normal * 0.5 + 0.5, resolved_base_color.a);
  if (debug_display == 3)
    return encode_output(resolved_metallic.xxx, resolved_base_color.a);
  if (debug_display == 4)
    return encode_output(roughness.xxx, resolved_base_color.a);
  if (debug_display == 5)
    return encode_output(geometric_normal * 0.5 + 0.5, resolved_base_color.a);
  if (debug_display == 6)
    return encode_output(sampled_normal * 0.5 + 0.5, resolved_base_color.a);
  if (debug_display == 7)
    return encode_output(normalize(input.vertex_normal) * 0.5 + 0.5, resolved_base_color.a);
  if (debug_display == 8)
    return encode_output(normalize(input.vertex_tangent) * 0.5 + 0.5,
                         resolved_base_color.a);
  return encode_output(color, resolved_base_color.a);
}
