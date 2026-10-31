// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_HPP_
#define GRANIT_RENDER_GRAPH_HPP_

#include <granit/render_graph.h>
#include <granit/core/result.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace granit {

class render_graph_handle {
public:
  render_graph_handle() noexcept = default;
  ~render_graph_handle() { static_cast<void>(reset()); }

  [[nodiscard]] result initialize(const granit_render_graph_desc& desc = GRANIT_RENDER_GRAPH_DESC_INIT) noexcept {
    static_cast<void>(reset());
    return from_native(granit_render_graph_create(&desc, &handle_));
  }

  render_graph_handle(const render_graph_handle&) = delete;
  render_graph_handle& operator=(const render_graph_handle&) = delete;
  render_graph_handle(render_graph_handle&& other) noexcept : handle_(other.handle_) {
    other.handle_ = GRANIT_NULL_HANDLE;
  }
  render_graph_handle& operator=(render_graph_handle&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      handle_ = other.handle_;
      other.handle_ = GRANIT_NULL_HANDLE;
    }
    return *this;
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_render_graph native_handle() const noexcept { return handle_; }
  [[nodiscard]] granit_result reset() noexcept {
    if (handle_ == GRANIT_NULL_HANDLE)
      return GRANIT_SUCCESS;
    const auto result = granit_render_graph_destroy(handle_);
    if (result == GRANIT_SUCCESS)
      handle_ = GRANIT_NULL_HANDLE;
    return result;
  }

private:
  granit_render_graph handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
