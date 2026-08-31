#version 450

layout(set = 0, binding = 0) uniform DrawData {
  vec4 translation;
  vec4 color;
} draw_data;

layout(location = 0) out vec4 vertex_color;
layout(location = 1) out vec2 vertex_uv;
layout(location = 2) out vec3 vertex_normal;

layout(location = 0) in vec2 vertex_position;
layout(location = 1) in vec2 vertex_uv_input;
layout(location = 2) in vec3 vertex_normal_input;

void main() {
  gl_Position = vec4(vertex_position + draw_data.translation.xy, 0.0, 1.0);
  vertex_color = draw_data.color;
  vertex_uv = vertex_uv_input;
  vertex_normal = vertex_normal_input;
}
