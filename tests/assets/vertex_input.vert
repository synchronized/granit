#version 450

layout(location = 0) in vec2 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 instance_offset;
layout(location = 0) out vec3 vertex_color;

void main() {
  gl_Position = vec4(position + instance_offset, 0.0, 1.0);
  vertex_color = color;
}
