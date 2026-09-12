// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

[[vk::binding(0, 0)]] RWStructuredBuffer<uint> output_buffer;

[numthreads(1, 1, 1)]
void compute_main(uint3 global_id : SV_DispatchThreadID) {
  output_buffer[global_id.x] = global_id.x * 3u + 7u;
}
