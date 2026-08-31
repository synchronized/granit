#version 450

layout(location = 0) in vec4 vertex_color;
layout(location = 1) in vec2 vertex_uv;
layout(location = 2) in vec3 vertex_normal;
layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 1) uniform texture2D base_color_texture;
layout(set = 0, binding = 2) uniform sampler base_color_sampler;
layout(set = 0, binding = 3) uniform texture2D normal_texture;
layout(set = 0, binding = 4) uniform texture2D metallic_roughness_texture;

void main() {
  const vec3 sampled_normal =
      texture(sampler2D(normal_texture, base_color_sampler), vertex_uv).xyz * 2.0 - 1.0;
  const vec2 metallic_roughness =
      texture(sampler2D(metallic_roughness_texture, base_color_sampler), vertex_uv).bg;
  const float material_scale =
      0.5 + 0.25 * max(dot(normalize(vertex_normal), sampled_normal), 0.0) +
      0.25 * metallic_roughness.x;
  output_color = texture(sampler2D(base_color_texture, base_color_sampler), vertex_uv) *
                 vertex_color * material_scale;
}
