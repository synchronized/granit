// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#version 460

layout(set = 1, binding = 0) uniform MaterialConstants { vec4 tint; }
material_constants;
layout(set = 1, binding = 1) uniform texture2D albedo_texture;
layout(set = 1, binding = 2) uniform sampler linear_sampler;
layout(constant_id = 7) const float gain = 1.0;

layout(location = 0) out vec4 output_color;

void main() {
  output_color = texture(sampler2D(albedo_texture, linear_sampler), vec2(0.5)) *
                 material_constants.tint * gain;
}
