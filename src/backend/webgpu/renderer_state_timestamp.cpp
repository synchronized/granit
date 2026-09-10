// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include "core/texture_format.h"

#include <new>
#include <string>
#include <vector>

namespace granit::detail {

granit_result webgpu_renderer_state::create_timestamp_query_pool(
    std::uint32_t query_count,
    std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept {
  return timestamps_ ? timestamps_->create(query_count, pool) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::read_timestamp_query_results(backend_timestamp_query_pool_resource& pool,
                                                    std::uint32_t first,
                                                    std::span<std::uint64_t> values) noexcept {
  return timestamps_ ? timestamps_->read(pool, first, values) : GRANIT_ERROR_UNSUPPORTED;
}

granit_result
webgpu_renderer_state::reset_timestamp_queries(backend_command_recorder_resource& recorder,
                                               backend_timestamp_query_pool_resource& pool,
                                               std::uint32_t first, std::uint32_t count) noexcept {
  if (!timestamps_ || !commands_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto command = commands_->native_recorder(recorder);
  const auto query = timestamps_->native_handle(pool);
  return command != 0 && query != 0
             ? provider_.recorder_reset_timestamp_queries(instance_, command, query, first, count)
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result webgpu_renderer_state::write_timestamp(backend_command_recorder_resource& recorder,
                                                     backend_timestamp_query_pool_resource& pool,
                                                     granit_timestamp_stage,
                                                     std::uint32_t index) noexcept {
  if (!timestamps_ || !commands_)
    return GRANIT_ERROR_UNSUPPORTED;
  const auto command = commands_->native_recorder(recorder);
  const auto query = timestamps_->native_handle(pool);
  return command != 0 && query != 0
             ? provider_.recorder_write_timestamp(instance_, command, query, index)
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

granit_result
webgpu_renderer_state::set_timestamp_query_pool_name(backend_timestamp_query_pool_resource&,
                                                     std::string_view) noexcept {
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
