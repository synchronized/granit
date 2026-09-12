// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

static const float PI = 3.14159265358979323846;

struct vertex_input {
  float3 position : POSITION;
  float3 normal : NORMAL;
  float4 tangent : TANGENT;
  float2 texture_coordinate : TEXCOORD0;
};

struct vertex_output {
  float4 position : SV_Position;
  float3 world_position : TEXCOORD0;
  float3 world_normal : TEXCOORD1;
  float4 world_tangent : TEXCOORD2;
  float2 texture_coordinate : TEXCOORD3;
  float3 vertex_normal : TEXCOORD4;
  float3 vertex_tangent : TEXCOORD5;
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
};

[[vk::binding(1, 1)]] Texture2D<float4> base_color_texture;
[[vk::binding(2, 1)]] Texture2D<float4> metallic_roughness_texture;
[[vk::binding(3, 1)]] Texture2D<float4> normal_texture;
[[vk::binding(4, 1)]] Texture2D<float4> occlusion_texture;
[[vk::binding(5, 1)]] Texture2D<float4> emissive_texture;
[[vk::binding(6, 1)]] SamplerState pbr_sampler;

[[vk::binding(0, 2)]] cbuffer ObjectConstants {
  column_major float4x4 model;
  column_major float4x4 normal_matrix;
  uint4 object_id;
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
  output.world_normal = normalize(mul(normal_matrix, float4(input.normal, 0.0)).xyz);
  output.world_tangent =
      float4(normalize(mul(model, float4(input.tangent.xyz, 0.0)).xyz), input.tangent.w);
  output.texture_coordinate = input.texture_coordinate;
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

float4 fragment_main(vertex_output input) : SV_Target0 {
  const float4 sampled_base_color =
      base_color_texture.Sample(pbr_sampler, input.texture_coordinate);
  const float4 resolved_base_color = base_color * sampled_base_color;
  const float4 sampled_metallic_roughness =
      metallic_roughness_texture.Sample(pbr_sampler, input.texture_coordinate);
  const float resolved_metallic = saturate(metallic * sampled_metallic_roughness.b);
  const float sampled_roughness =
      clamp(perceptual_roughness * sampled_metallic_roughness.g, 0.045, 1.0);

  const float3 tangent = normalize(input.world_tangent.xyz);
  const float3 geometric_normal = normalize(input.world_normal);
  const float3 bitangent = normalize(cross(geometric_normal, tangent)) * input.world_tangent.w;
  const float3 sampled_normal =
      normal_texture.Sample(pbr_sampler, input.texture_coordinate).xyz * 2.0 - 1.0;
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
  const float occlusion_sample = occlusion_texture.Sample(pbr_sampler, input.texture_coordinate).r;
  const float occlusion = lerp(1.0, occlusion_sample, occlusion_strength);
  const float3 resolved_emissive =
      emissive * emissive_texture.Sample(pbr_sampler, input.texture_coordinate).rgb;

  if (debug_display == 1)
    return float4(resolved_base_color.rgb, resolved_base_color.a);
  if (debug_display == 2)
    return float4(normal * 0.5 + 0.5, 1.0);
  if (debug_display == 3)
    return float4(resolved_metallic.xxx, 1.0);
  if (debug_display == 4)
    return float4(roughness.xxx, 1.0);
  if (debug_display == 5)
    return float4(geometric_normal * 0.5 + 0.5, 1.0);
  if (debug_display == 6)
    return float4(sampled_normal * 0.5 + 0.5, 1.0);
  if (debug_display == 7)
    return float4(normalize(input.vertex_normal) * 0.5 + 0.5, 1.0);
  if (debug_display == 8)
    return float4(normalize(input.vertex_tangent) * 0.5 + 0.5, 1.0);

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
  return float4(color, resolved_base_color.a);
}
