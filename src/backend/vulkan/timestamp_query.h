// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_VULKAN_TIMESTAMP_QUERY_H_
#define GRANIT_BACKEND_VULKAN_TIMESTAMP_QUERY_H_

#include <cstdint>
#include <span>

#include <granit/core/result.h>

#include <volk.h>

namespace granit::detail {

class vulkan_device;

/** Vulkan 时间戳查询池；结果统一换算为纳秒，不向公共接口传播 Vulkan 单位。 */
class vulkan_timestamp_query_pool {
public:
  vulkan_timestamp_query_pool() = default;
  ~vulkan_timestamp_query_pool() = default;
  vulkan_timestamp_query_pool(const vulkan_timestamp_query_pool&) = delete;
  vulkan_timestamp_query_pool& operator=(const vulkan_timestamp_query_pool&) = delete;

  [[nodiscard]] granit_result initialize(const vulkan_device& device,
                                         std::uint32_t query_count) noexcept;
  [[nodiscard]] granit_result reset(const vulkan_device& device, VkCommandBuffer commands,
                                    std::uint32_t first, std::uint32_t count) noexcept;
  [[nodiscard]] granit_result write(const vulkan_device& device, VkCommandBuffer commands,
                                    VkPipelineStageFlags2 stage, std::uint32_t index) noexcept;
  [[nodiscard]] granit_result read_nanoseconds(const vulkan_device& device, std::uint32_t first,
                                               std::span<std::uint64_t> values,
                                               bool wait) const noexcept;
  void destroy(const vulkan_device& device) noexcept;

  [[nodiscard]] bool valid() const noexcept { return pool_ != VK_NULL_HANDLE; }
  [[nodiscard]] std::uint32_t query_count() const noexcept { return query_count_; }

private:
  VkQueryPool pool_{VK_NULL_HANDLE};
  std::uint32_t query_count_{};
  float timestamp_period_{};
};

} // namespace granit::detail

#endif
