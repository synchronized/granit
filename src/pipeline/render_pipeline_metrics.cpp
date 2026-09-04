// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/render_pipeline_metrics.h"

#include <granit/renderer/command_recorder.h>
#include <granit/renderer/timestamp_query.h>

#include <array>

namespace granit::pipeline::detail {

granit_result publish_render_pipeline_metrics(render_pipeline_state& state,
                                              render_pipeline_state::metrics_slot& slot) {
  if (!slot.pending)
    return GRANIT_ERROR_NOT_READY;
  std::array<std::uint64_t, 8> values{};
  const auto result = granit_timestamp_query_pool_get_results(
      state.renderer, slot.pool, 0, static_cast<std::uint32_t>(values.size()), values.data());
  if (result != GRANIT_SUCCESS)
    return result;
  state.metrics.sample_sequence += 1;
  state.metrics.shadow_gpu_ns = values[1] - values[0];
  state.metrics.opaque_gpu_ns = values[3] - values[2];
  state.metrics.tone_mapping_gpu_ns = values[5] - values[4];
  state.metrics.total_gpu_ns = values[7] - values[6];
  state.metrics_available = true;
  slot.pending = false;
  return GRANIT_SUCCESS;
}

granit_timestamp_query_pool prepare_render_pipeline_metrics_slot(render_pipeline_state& state,
                                                                 std::uint32_t frame_slot,
                                                                 std::uint32_t frame_slot_count) {
  if (!state.metrics_enabled || frame_slot_count == 0 || frame_slot >= frame_slot_count)
    return GRANIT_NULL_HANDLE;
  if (state.metrics_slots.size() < frame_slot_count)
    state.metrics_slots.resize(frame_slot_count);
  auto& slot = state.metrics_slots[frame_slot];
  if (slot.pool == GRANIT_NULL_HANDLE) {
    const granit_timestamp_query_pool_desc desc{sizeof(desc), 8, 0};
    if (granit_timestamp_query_pool_create(state.renderer, &desc, &slot.pool) != GRANIT_SUCCESS)
      return GRANIT_NULL_HANDLE;
  }
  if (slot.pending && publish_render_pipeline_metrics(state, slot) != GRANIT_SUCCESS)
    return GRANIT_NULL_HANDLE;
  return slot.pool;
}

} // namespace granit::pipeline::detail
