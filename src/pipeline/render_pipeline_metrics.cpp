// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/render_pipeline_metrics.h"

#include <granit/renderer/async_operation.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/timestamp_query.h>

#include <array>

namespace granit::pipeline::detail {

namespace {

void clear_operation(render_pipeline_state& state, render_pipeline_state::metrics_slot& slot) {
  if (slot.operation != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_async_operation_destroy(state.renderer, slot.operation));
  slot.operation = GRANIT_NULL_HANDLE;
}

void poll_slot(render_pipeline_state& state, render_pipeline_state::metrics_slot& slot) {
  if (slot.operation == GRANIT_NULL_HANDLE)
    return;
  granit_async_operation_status status = GRANIT_ASYNC_OPERATION_STATUS_INIT;
  const auto status_result =
      granit_async_operation_get_status(state.renderer, slot.operation, &status);
  if (status_result != GRANIT_SUCCESS) {
    clear_operation(state, slot);
    return;
  }
  if (status.state == GRANIT_ASYNC_OPERATION_STATE_PENDING ||
      status.state == GRANIT_ASYNC_OPERATION_STATE_RUNNING) {
    return;
  }
  if (status.state != GRANIT_ASYNC_OPERATION_STATE_SUCCEEDED) {
    clear_operation(state, slot);
    return;
  }
  std::array<std::uint64_t, 8> values{};
  const auto copy_result = granit_timestamp_query_pool_copy_results(
      state.renderer, slot.pool, slot.operation, values.data(),
      static_cast<std::uint32_t>(values.size()));
  clear_operation(state, slot);
  if (copy_result != GRANIT_SUCCESS || values[1] < values[0] || values[3] < values[2] ||
      values[5] < values[4] || values[7] < values[6] ||
      slot.submission_sequence <= state.published_metrics_submission_sequence) {
    return;
  }
  state.metrics.sample_sequence += 1;
  state.metrics.shadow_gpu_ns = values[1] - values[0];
  state.metrics.opaque_gpu_ns = values[3] - values[2];
  state.metrics.tone_mapping_gpu_ns = values[5] - values[4];
  state.metrics.total_gpu_ns = values[7] - values[6];
  state.published_metrics_submission_sequence = slot.submission_sequence;
  state.metrics_available = true;
}

} // namespace

void poll_render_pipeline_metrics(render_pipeline_state& state) {
  for (auto& slot : state.metrics_slots)
    poll_slot(state, slot);
}

void begin_render_pipeline_metrics_read(render_pipeline_state& state,
                                        render_pipeline_state::metrics_slot& slot) {
  if (slot.pool == GRANIT_NULL_HANDLE || slot.operation != GRANIT_NULL_HANDLE)
    return;
  granit_async_operation operation = GRANIT_NULL_HANDLE;
  const auto result =
      granit_timestamp_query_pool_get_results_async(state.renderer, slot.pool, 0, 8, &operation);
  if (result != GRANIT_SUCCESS)
    return;
  slot.operation = operation;
  slot.submission_sequence = state.next_metrics_submission_sequence++;
}

granit_result release_render_pipeline_metrics_slot(render_pipeline_state& state,
                                                   render_pipeline_state::metrics_slot& slot) {
  auto result = GRANIT_SUCCESS;
  if (slot.operation != GRANIT_NULL_HANDLE) {
    const auto cancel_result =
        granit_async_operation_request_cancel(state.renderer, slot.operation);
    if (cancel_result != GRANIT_SUCCESS)
      result = cancel_result;
    const auto destroy_result = granit_async_operation_destroy(state.renderer, slot.operation);
    if (result == GRANIT_SUCCESS)
      result = destroy_result;
    slot.operation = GRANIT_NULL_HANDLE;
  }
  if (slot.pool != GRANIT_NULL_HANDLE) {
    const auto destroy_result = granit_timestamp_query_pool_destroy(state.renderer, slot.pool);
    if (result == GRANIT_SUCCESS)
      result = destroy_result;
    slot.pool = GRANIT_NULL_HANDLE;
  }
  return result;
}

granit_timestamp_query_pool prepare_render_pipeline_metrics_slot(render_pipeline_state& state,
                                                                 std::uint32_t frame_slot,
                                                                 std::uint32_t frame_slot_count) {
  if (!state.metrics_enabled || frame_slot_count == 0 || frame_slot >= frame_slot_count)
    return GRANIT_NULL_HANDLE;
  poll_render_pipeline_metrics(state);
  if (state.metrics_slots.size() < frame_slot_count)
    state.metrics_slots.resize(frame_slot_count);
  auto& slot = state.metrics_slots[frame_slot];
  if (slot.pool == GRANIT_NULL_HANDLE) {
    const granit_timestamp_query_pool_desc desc{sizeof(desc), 8, 0};
    if (granit_timestamp_query_pool_create(state.renderer, &desc, &slot.pool) != GRANIT_SUCCESS)
      return GRANIT_NULL_HANDLE;
  }
  if (slot.operation != GRANIT_NULL_HANDLE)
    return GRANIT_NULL_HANDLE;
  return slot.pool;
}

} // namespace granit::pipeline::detail
