#version 450

layout(set = 0, binding = 0) uniform DrawData {
  vec4 translation;
  vec4 color;
} draw_data;

layout(location = 0) out vec4 vertex_color;

void main() {
  const vec2 positions[3] = vec2[3](
      vec2(-0.22, -0.30), vec2(0.22, -0.30), vec2(0.0, 0.30));
  gl_Position = vec4(positions[gl_VertexIndex] + draw_data.translation.xy, 0.0, 1.0);
  vertex_color = draw_data.color;
}
