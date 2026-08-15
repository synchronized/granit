// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_DEBUG_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_DEBUG_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/debug_draw_list.h>

#include <limits>
#include <span>
#include <utility>

namespace granit {

class debug_draw_list {
public:
  debug_draw_list() = default;
  ~debug_draw_list() { static_cast<void>(destroy()); }
  debug_draw_list(const debug_draw_list&) = delete;
  debug_draw_list& operator=(const debug_draw_list&) = delete;
  debug_draw_list(debug_draw_list&& other) noexcept { move_from(other); }
  debug_draw_list& operator=(debug_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_debug_draw_list_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_debug_draw_list_create(renderer, &desc, &handle_));
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_debug_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append_lines(std::span<const granit_debug_draw_line> lines) noexcept {
    if (lines.size() > std::numeric_limits<uint32_t>::max())
      return result::invalid_argument;
    return from_native(granit_debug_draw_list_append_lines(renderer_, handle_, lines.data(),
                                                           static_cast<uint32_t>(lines.size())));
  }
  [[nodiscard]] result
  append_triangles(std::span<const granit_debug_draw_triangle> triangles) noexcept {
    if (triangles.size() > std::numeric_limits<uint32_t>::max())
      return result::invalid_argument;
    return from_native(granit_debug_draw_list_append_triangles(
        renderer_, handle_, triangles.data(), static_cast<uint32_t>(triangles.size())));
  }
  [[nodiscard]] result get_stats(granit_debug_draw_list_stats& stats) const noexcept {
    return from_native(granit_debug_draw_list_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result append_screen_to_canvas(granit_canvas_draw_list canvas) noexcept {
    return from_native(granit_debug_draw_list_append_screen_to_canvas(renderer_, handle_, canvas));
  }
  [[nodiscard]] result record_world(granit_command_recorder recorder,
                                    const granit_debug_draw_record_desc& desc) noexcept {
    return from_native(granit_debug_draw_list_record_world(renderer_, recorder, handle_, &desc));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_debug_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_debug_draw_list native_handle() const noexcept { return handle_; }

private:
  void move_from(debug_draw_list& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
  }
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_debug_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
