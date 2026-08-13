// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/timestamp_query.h"

#include "backend/vulkan/device.h"
#include "backend/vulkan/result.h"

#include <cmath>
#include <limits>
#include <vector>

namespace granit::detail {

granit_result vulkan_timestamp_query_pool::initialize(const vulkan_device& device,
                                                      std::uint32_t query_count) noexcept {
  if (valid() || !device.valid() || query_count < 2 ||
      !(device.properties().limits.timestampPeriod > 0.0F))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  VkQueryPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
  info.queryType = VK_QUERY_TYPE_TIMESTAMP;
  info.queryCount = query_count;
  const auto result =
      device.functions().vkCreateQueryPool(device.native_handle(), &info, nullptr, &pool_);
  if (result != VK_SUCCESS) {
    pool_ = VK_NULL_HANDLE;
    return map_vulkan_result(result);
  }
  query_count_ = query_count;
  timestamp_period_ = device.properties().limits.timestampPeriod;
  return GRANIT_SUCCESS;
}

granit_result vulkan_timestamp_query_pool::reset(const vulkan_device& device,
                                                 VkCommandBuffer commands, std::uint32_t first,
                                                 std::uint32_t count) noexcept {
  if (!valid() || !device.valid() || commands == VK_NULL_HANDLE || count == 0 ||
      first > query_count_ || count > query_count_ - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  device.functions().vkCmdResetQueryPool(commands, pool_, first, count);
  return GRANIT_SUCCESS;
}

granit_result vulkan_timestamp_query_pool::write(const vulkan_device& device,
                                                 VkCommandBuffer commands,
                                                 VkPipelineStageFlags2 stage,
                                                 std::uint32_t index) noexcept {
  if (!valid() || !device.valid() || commands == VK_NULL_HANDLE || stage == 0 ||
      index >= query_count_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  device.functions().vkCmdWriteTimestamp2(commands, stage, pool_, index);
  return GRANIT_SUCCESS;
}

granit_result vulkan_timestamp_query_pool::read_nanoseconds(const vulkan_device& device,
                                                            std::uint32_t first,
                                                            std::span<std::uint64_t> values,
                                                            bool wait) const noexcept {
  if (!valid() || !device.valid() || values.empty() || first > query_count_ ||
      values.size() > query_count_ - first)
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<std::uint64_t> ticks;
  try {
    ticks.resize(values.size());
  } catch (...) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT;
  if (wait)
    flags |= VK_QUERY_RESULT_WAIT_BIT;
  const auto result = device.functions().vkGetQueryPoolResults(
      device.native_handle(), pool_, first, static_cast<std::uint32_t>(ticks.size()),
      ticks.size() * sizeof(std::uint64_t), ticks.data(), sizeof(std::uint64_t), flags);
  if (result != VK_SUCCESS)
    return map_vulkan_result(result);

  for (std::size_t index = 0; index < ticks.size(); ++index) {
    const auto nanoseconds = static_cast<long double>(ticks[index]) * timestamp_period_;
    if (!std::isfinite(nanoseconds) ||
        nanoseconds > static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
      return GRANIT_ERROR_INTERNAL;
    values[index] = static_cast<std::uint64_t>(std::llround(nanoseconds));
  }
  return GRANIT_SUCCESS;
}

void vulkan_timestamp_query_pool::destroy(const vulkan_device& device) noexcept {
  if (pool_ != VK_NULL_HANDLE && device.valid())
    device.functions().vkDestroyQueryPool(device.native_handle(), pool_, nullptr);
  pool_ = VK_NULL_HANDLE;
  query_count_ = 0;
  timestamp_period_ = 0.0F;
}

} // namespace granit::detail
