struct OutputBuffer {
  values : array<u32>,
}

@group(0u) @binding(0u) var<storage, read_write> output_buffer : OutputBuffer;

@compute @workgroup_size(1u, 1u, 1u)
fn main(@builtin(global_invocation_id) gl_GlobalInvocationID : vec3<u32>) {
  let index = gl_GlobalInvocationID.x;
  output_buffer.values[index] = ((index * 3u) + 7u);
}
