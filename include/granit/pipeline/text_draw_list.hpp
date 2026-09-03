// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_TEXT_DRAW_LIST_HPP_
#define GRANIT_PIPELINE_TEXT_DRAW_LIST_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/text_draw_list.h>

#include <limits>
#include <span>
#include <utility>

namespace granit {

/** 公共 Text Draw List C ABI 的轻量 move-only RAII 包装。 */
class text_draw_list {
public:
  text_draw_list() = default;
  ~text_draw_list() { static_cast<void>(destroy()); }
  text_draw_list(const text_draw_list&) = delete;
  text_draw_list& operator=(const text_draw_list&) = delete;
  text_draw_list(text_draw_list&& other) noexcept { move_from(other); }
  text_draw_list& operator=(text_draw_list&& other) noexcept {
    if (this != &other) {
      static_cast<void>(destroy());
      move_from(other);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer,
                                  const granit_text_draw_list_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_text_draw_list_create(renderer, &desc, &handle_));
    if (value.ok())
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result clear() noexcept {
    return from_native(granit_text_draw_list_clear(renderer_, handle_));
  }
  [[nodiscard]] result append_glyph_run(std::span<const granit_text_glyph_instance> glyphs,
                                        granit_scissor scissor = {}) noexcept {
    if (glyphs.size() > std::numeric_limits<uint32_t>::max())
      return result::invalid_argument;
    granit_text_glyph_run_desc desc = GRANIT_TEXT_GLYPH_RUN_DESC_INIT;
    desc.glyph_count = static_cast<uint32_t>(glyphs.size());
    desc.glyphs = glyphs.data();
    desc.scissor = scissor;
    return from_native(granit_text_draw_list_append_glyph_run(renderer_, handle_, &desc));
  }
  [[nodiscard]] result get_stats(granit_text_draw_list_stats& stats) const noexcept {
    return from_native(granit_text_draw_list_get_stats(renderer_, handle_, &stats));
  }
  [[nodiscard]] result append_to_canvas(granit_text_atlas atlas,
                                        granit_canvas_draw_list canvas) const noexcept {
    return from_native(granit_text_draw_list_append_to_canvas(renderer_, handle_, atlas, canvas));
  }
  [[nodiscard]] result destroy() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_text_draw_list_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_text_draw_list native_handle() const noexcept { return handle_; }

private:
  void move_from(text_draw_list& other) noexcept {
    renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
    handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
  }

  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_text_draw_list handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
