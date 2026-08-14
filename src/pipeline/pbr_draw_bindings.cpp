// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "pipeline/pbr_draw_bindings.h"

#include <array>
#include <span>

namespace granit::pipeline::detail {
namespace {

template <typename T> std::span<const std::byte> bytes(const T& value) noexcept {
  return std::as_bytes(std::span{&value, 1});
}

} // namespace

granit_result
pbr_draw_bindings::initialize(granit_renderer renderer, const material_draw_state& material,
                              const granit::material::pbr_frame_constants& frame,
                              const granit::material::pbr_object_constants& object) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized() ||
      material.frame_layout == GRANIT_NULL_HANDLE || material.object_layout == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto make_buffer = [&](granit::buffer& buffer, const auto& value) {
    return buffer.initialize(
        renderer,
        {.size = sizeof(value),
         .usage = granit::buffer_usage::uniform | granit::buffer_usage::transfer_destination,
         .location = granit::memory_location::upload},
        bytes(value));
  };
  auto result = make_buffer(frame_buffer_, frame);
  if (granit::succeeded(result))
    result = make_buffer(object_buffer_, object);
  const std::array frame_entry{granit::bind_group_entry{
      .binding = 0, .resource = frame_buffer_.native_handle(), .offset = 0, .size = sizeof(frame)}};
  if (granit::succeeded(result))
    result = frame_group_.initialize(renderer, material.frame_layout, frame_entry);
  const std::array object_entry{granit::bind_group_entry{.binding = 0,
                                                         .resource = object_buffer_.native_handle(),
                                                         .offset = 0,
                                                         .size = sizeof(object)}};
  if (granit::succeeded(result))
    result = object_group_.initialize(renderer, material.object_layout, object_entry);
  if (granit::failed(result))
    static_cast<void>(reset());
  return static_cast<granit_result>(result);
}

granit_result
pbr_draw_bindings::update(const granit::material::pbr_frame_constants& frame,
                          const granit::material::pbr_object_constants& object) noexcept {
  if (!initialized())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = frame_buffer_.write(0, bytes(frame));
  if (granit::succeeded(result))
    result = object_buffer_.write(0, bytes(object));
  return static_cast<granit_result>(result);
}

granit_result pbr_draw_bindings::reset() noexcept {
  auto first = static_cast<granit_result>(object_group_.reset());
  const auto capture = [&](granit::result result) {
    if (first == GRANIT_SUCCESS && granit::failed(result))
      first = static_cast<granit_result>(result);
  };
  capture(frame_group_.reset());
  capture(object_buffer_.reset());
  capture(frame_buffer_.reset());
  return first;
}

} // namespace granit::pipeline::detail
