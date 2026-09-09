struct VertexOutput {
  @builtin(position) position : vec4<f32>,
  @location(0u) color : vec3<f32>,
}

@vertex
fn main(@location(0u) position : vec2<f32>,
        @location(1u) color : vec3<f32>) -> VertexOutput {
  return VertexOutput(vec4<f32>(position, 0.0f, 1.0f), color);
}
