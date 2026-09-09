@fragment
fn main(@location(0u) color : vec3<f32>) -> @location(0u) vec4<f32> {
  return vec4<f32>(color, 1.0f);
}
