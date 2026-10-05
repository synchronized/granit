// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

struct object_data { float4 position_size; float4 color_mode; };
StructuredBuffer<object_data> objects : register(t0, space0);
struct output_data { float4 position : SV_Position; float4 color : COLOR0; };

output_data vertex_main(uint vertex_id : SV_VertexID) {
  uint object_id = vertex_id / 6;
  uint corner_id = vertex_id % 6;
  const float2 corners[6] = {float2(-1,-1), float2(1,-1), float2(1,1),
                             float2(-1,-1), float2(1,1), float2(-1,1)};
  object_data object = objects[object_id];
  output_data output;
  output.position = float4(object.position_size.xy + corners[corner_id] * object.position_size.z,
                           object.position_size.w, 1.0);
  output.color = object.color_mode;
  return output;
}

float4 fragment_main(output_data input) : SV_Target0 {
  if (input.color.a < 0.5) discard;
  return input.color;
}
