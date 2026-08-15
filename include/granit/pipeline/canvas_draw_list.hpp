// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_CANVAS_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_CANVAS_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/canvas_draw_list.h>

#include <limits>
#include <span>
#include <utility>

namespace granit {

/** 公共 Canvas Draw List C ABI 的轻量 move-only RAII 包装。 */
class canvas_draw_list {
public:
  canvas_draw_list() = default;
  ~canvas_draw_list() { static_cast<void>(destroy()); }
  canvas_draw_list(const canvas_draw_list&) = delete;
  canvas_draw_list& operator=(const canvas_draw_list&) = delete;
  canvas_draw_list(canvas_draw_list&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  canvas_draw_list& operator=(canvas_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_canvas_draw_list_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_canvas_draw_list_create(renderer, &desc, &handle_));
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_canvas_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append(std::span<const granit_canvas_vertex> vertices,
                              std::span<const std::uint32_t> indices,
                              const granit_canvas_draw_state& state) noexcept {
    if (vertices.size() > std::numeric_limits<std::uint32_t>::max() ||
        indices.size() > std::numeric_limits<std::uint32_t>::max()) {
      return result::invalid_argument;
    }
    return from_native(granit_canvas_draw_list_append(
        renderer_, handle_, vertices.data(), static_cast<std::uint32_t>(vertices.size()),
        indices.data(), static_cast<std::uint32_t>(indices.size()), &state));
  }
  [[nodiscard]] result append_rect(const granit_canvas_rect_desc& desc) noexcept {
    return from_native(granit_canvas_draw_list_append_rect(renderer_, handle_, &desc));
  }
  [[nodiscard]] result get_stats(granit_canvas_draw_list_stats& stats) const noexcept {
    return from_native(granit_canvas_draw_list_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result record(granit_command_recorder recorder,
                              const granit_canvas_record_desc& desc) noexcept {
    return from_native(granit_canvas_draw_list_record(renderer_, recorder, handle_, &desc));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_canvas_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_canvas_draw_list native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_canvas_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
