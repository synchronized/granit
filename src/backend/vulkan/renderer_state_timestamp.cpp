// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderer_state.h"
#include "backend/vulkan/renderer_state_utils.h"

#include "backend/vulkan/resources.h"
#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"
#include "core/texture_format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <limits>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace granit::detail {

granit_result vulkan_renderer_state::create_timestamp_query_pool(
    std::uint32_t query_count,
    std::unique_ptr<backend_timestamp_query_pool_resource>& pool) noexcept {
  pool.reset();
  try {
    auto resource = std::make_unique<vulkan_timestamp_query_pool_resource>(shared_from_this());
    const auto result = resource->native().initialize(device_, query_count);
    if (result != GRANIT_SUCCESS)
      return result;
    pool = std::move(resource);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
vulkan_renderer_state::read_timestamp_query_results(backend_timestamp_query_pool_resource& resource,
                                                    std::uint32_t first,
                                                    std::span<std::uint64_t> values) noexcept {
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(resource).native();
  return pool.read_nanoseconds(device_, first, values, false);
}

granit_result
vulkan_renderer_state::reset_timestamp_queries(backend_command_recorder_resource& recorder_resource,
                                               backend_timestamp_query_pool_resource& pool_resource,
                                               std::uint32_t first, std::uint32_t count) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(pool_resource).native();
  return pool.reset(device_, recorder.native_handle(), first, count);
}

granit_result
vulkan_renderer_state::write_timestamp(backend_command_recorder_resource& recorder_resource,
                                       backend_timestamp_query_pool_resource& pool_resource,
                                       granit_timestamp_stage stage, std::uint32_t index) noexcept {
  const auto native_stage =
      stage == GRANIT_TIMESTAMP_STAGE_TOP      ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_DRAW   ? VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT
      : stage == GRANIT_TIMESTAMP_STAGE_BOTTOM ? VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT
                                               : VkPipelineStageFlags2{};
  if (native_stage == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(recorder_resource).native();
  auto& pool = static_cast<vulkan_timestamp_query_pool_resource&>(pool_resource).native();
  return pool.write(device_, recorder.native_handle(), native_stage, index);
}

granit_result vulkan_renderer_state::set_timestamp_query_pool_name(
    backend_timestamp_query_pool_resource& resource, std::string_view name) noexcept {
  const auto native =
      static_cast<vulkan_timestamp_query_pool_resource&>(resource).native().native_handle();
  return set_object_name(VK_OBJECT_TYPE_QUERY_POOL, object_handle_value(native), name);
}

} // namespace granit::detail
