// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "lighting/light_buffers.h"

#include <algorithm>
#include <array>
#include <limits>

namespace granit::lighting {
namespace {

template <typename T> std::span<const std::byte> bytes(const T& value) noexcept {
  return {reinterpret_cast<const std::byte*>(&value), sizeof(value)};
}

template <typename T> std::span<const std::byte> bytes(std::span<const T> values) noexcept {
  return {reinterpret_cast<const std::byte*>(values.data()), values.size_bytes()};
}

bool valid(const light_limits& value) noexcept {
  return value.directional <= maximum_directional_lights && value.point <= maximum_point_lights &&
         value.spot <= maximum_spot_lights;
}

template <typename T> std::uint64_t allocation_size(std::uint32_t capacity) noexcept {
  return sizeof(T) * static_cast<std::uint64_t>(std::max(capacity, 1U));
}

} // namespace

granit_result light_buffers::initialize(granit_renderer renderer, const light_limits& capacities,
                                        granit::memory_location memory_location) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() || !valid(capacities))
    return GRANIT_ERROR_INVALID_ARGUMENT;

  constexpr auto usage = granit::buffer_usage::storage | granit::buffer_usage::transfer_destination;
  const gpu_light_counts zero_counts{};
  auto result = counts_.initialize(
      renderer,
      {.size = sizeof(zero_counts),
       .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
       .location = memory_location},
      bytes(zero_counts));
  if (result.ok()) {
    result = directional_.initialize(
        renderer, {.size = allocation_size<gpu_directional_light>(capacities.directional),
                   .usage = usage,
                   .location = memory_location});
  }
  if (result.ok()) {
    result =
        point_.initialize(renderer, {.size = allocation_size<gpu_point_light>(capacities.point),
                                     .usage = usage,
                                     .location = memory_location});
  }
  if (result.ok()) {
    result = spot_.initialize(renderer, {.size = allocation_size<gpu_spot_light>(capacities.spot),
                                         .usage = usage,
                                         .location = memory_location});
  }
  if (result.failed()) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  capacities_ = capacities;
  return GRANIT_SUCCESS;
}

granit_result light_buffers::update(const packed_view_lights& lights) noexcept {
  if (!initialized() || lights.directional.size() > capacities_.directional ||
      lights.point.size() > capacities_.point || lights.spot.size() > capacities_.spot ||
      lights.directional.size() > std::numeric_limits<std::uint32_t>::max() ||
      lights.point.size() > std::numeric_limits<std::uint32_t>::max() ||
      lights.spot.size() > std::numeric_limits<std::uint32_t>::max())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  const gpu_light_counts counts{.directional =
                                    static_cast<std::uint32_t>(lights.directional.size()),
                                .point = static_cast<std::uint32_t>(lights.point.size()),
                                .spot = static_cast<std::uint32_t>(lights.spot.size())};
  auto result = granit::result::success;
  if (!lights.directional.empty())
    result = directional_.write(0, bytes(std::span{lights.directional}));
  if (result.ok() && !lights.point.empty())
    result = point_.write(0, bytes(std::span{lights.point}));
  if (result.ok() && !lights.spot.empty())
    result = spot_.write(0, bytes(std::span{lights.spot}));
  // 最后公布计数，避免 Shader 在成功更新前读取尚未写完的新数组范围。
  if (result.ok())
    result = counts_.write(0, bytes(counts));
  return static_cast<granit_result>(result);
}

granit_result light_buffers::reset() noexcept {
  granit_result first = GRANIT_SUCCESS;
  const auto capture = [&](granit::result value) {
    if (first == GRANIT_SUCCESS && value.failed())
      first = static_cast<granit_result>(value);
  };
  capture(spot_.reset());
  capture(point_.reset());
  capture(directional_.reset());
  capture(counts_.reset());
  capacities_ = {};
  return first;
}

} // namespace granit::lighting
