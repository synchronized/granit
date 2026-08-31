#version 450

layout(location = 0) in vec4 vertex_color;
layout(location = 1) in vec2 vertex_uv;
layout(location = 0) out vec4 output_color;

layout(set = 0, binding = 1) uniform texture2D base_color_texture;
layout(set = 0, binding = 2) uniform sampler base_color_sampler;

void main() {
  output_color = texture(sampler2D(base_color_texture, base_color_sampler), vertex_uv) *
                 vertex_color;
}
