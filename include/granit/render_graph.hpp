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
  [[nodiscard]] result import_buffer(granit_buffer buffer, bool exported, std::string_view name,
                                     granit_render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_import_buffer(
        handle_, buffer, exported ? 1U : 0U, name.data(), static_cast<std::uint32_t>(name.size()),
        &resource));
  }
  [[nodiscard]] result import_texture_view(granit_texture_view view, bool exported,
                                           std::string_view name,
                                           granit_render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_import_texture_view(
        handle_, view, exported ? 1U : 0U, name.data(), static_cast<std::uint32_t>(name.size()),
        &resource));
  }
  [[nodiscard]] result create_transient_buffer(const granit_buffer_desc& desc,
                                               std::string_view name,
                                               granit_render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_create_transient_buffer(
        handle_, &desc, name.data(), static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result create_transient_texture(const granit_texture_desc& desc,
                                                std::string_view name,
                                                granit_render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_create_transient_texture(
        handle_, &desc, name.data(), static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result update_transient_texture(
      const granit_render_graph_transient_texture_update& update) noexcept {
    return from_native(granit_render_graph_update_transient_texture(handle_, &update));
  }
  [[nodiscard]] result add_pass(const granit_render_graph_pass_desc& desc,
                                granit_render_graph_record_callback callback, void* user_data,
                                granit_render_graph_pass& pass) noexcept {
    return from_native(granit_render_graph_add_pass(handle_, &desc, callback, user_data, &pass));
  }
  [[nodiscard]] result add_dependency(granit_render_graph_pass before,
                                      granit_render_graph_pass after) noexcept {
    return from_native(granit_render_graph_add_dependency(handle_, before, after));
  }
  [[nodiscard]] result compile() noexcept {
    return from_native(granit_render_graph_compile(handle_));
  }
  [[nodiscard]] result execute(granit_renderer renderer, granit_frame frame,
                               std::uint32_t frame_slot,
                               granit_render_graph_execution_result& output) noexcept {
    return from_native(
        granit_render_graph_execute(handle_, renderer, frame, frame_slot, &output));
  }
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
