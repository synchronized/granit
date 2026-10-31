// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDER_GRAPH_HPP_
#define GRANIT_RENDER_GRAPH_HPP_

#include <granit/core/result.hpp>
#include <granit/render_graph/render_graph.h>
#include <granit/render_graph/types.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/texture.hpp>

#include <cstdint>
#include <new>
#include <span>
#include <string_view>
#include <vector>

namespace granit::render_graph {

class graph {
public:
  graph() noexcept = default;
  ~graph() { static_cast<void>(reset()); }

  [[nodiscard]] result initialize(const render_graph_desc& desc = {}) noexcept {
    static_cast<void>(reset());
    const granit_render_graph_desc native_desc{
        .struct_size = GRANIT_RENDER_GRAPH_DESC_VERSION_1_SIZE,
        .flags = desc.flags,
        .reserved = 0,
    };
    return from_native(granit_render_graph_create(&native_desc, &handle_));
  }

  graph(const graph&) = delete;
  graph& operator=(const graph&) = delete;
  graph(graph&& other) noexcept : handle_(other.handle_) { other.handle_ = GRANIT_NULL_HANDLE; }
  graph& operator=(graph&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      handle_ = other.handle_;
      other.handle_ = GRANIT_NULL_HANDLE;
    }
    return *this;
  }

  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] render_graph_native_handle native_handle() const noexcept { return handle_; }

  [[nodiscard]] result import_buffer(buffer_ref buffer, bool exported, std::string_view name,
                                     render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_import_buffer(
        handle_, buffer.native_handle(), exported ? 1U : 0U, name.data(),
        static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result import_texture_view(texture_view_ref view, bool exported,
                                           std::string_view name,
                                           render_graph_resource& resource) noexcept {
    return from_native(granit_render_graph_import_texture_view(
        handle_, view.native_handle(), exported ? 1U : 0U, name.data(),
        static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result create_transient_buffer(const buffer_desc& desc, std::string_view name,
                                               render_graph_resource& resource) noexcept {
    const granit_buffer_desc native_desc{
        .struct_size = GRANIT_BUFFER_DESC_VERSION_1_SIZE,
        .usage = static_cast<granit_buffer_usage>(desc.usage),
        .memory_location = static_cast<granit_memory_location>(desc.location),
        .reserved = 0,
        .size = desc.size,
        .reserved_2 = 0,
    };
    return from_native(granit_render_graph_create_transient_buffer(
        handle_, &native_desc, name.data(), static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result create_transient_texture(const texture_desc& desc, std::string_view name,
                                                render_graph_resource& resource) noexcept {
    const granit_texture_desc native_desc{
        .struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE,
        .dimension = static_cast<granit_texture_dimension>(desc.dimension),
        .format = static_cast<granit_texture_format>(desc.format),
        .usage = static_cast<granit_texture_usage>(desc.usage),
        .memory_location = static_cast<granit_memory_location>(desc.location),
        .width = desc.width,
        .height = desc.height,
        .depth = desc.depth,
        .mip_levels = desc.mip_levels,
        .array_layers = desc.array_layers,
        .sample_count = static_cast<granit_sample_count>(desc.samples),
        .reserved = 0,
    };
    return from_native(granit_render_graph_create_transient_texture(
        handle_, &native_desc, name.data(), static_cast<std::uint32_t>(name.size()), &resource));
  }
  [[nodiscard]] result
  update_transient_texture(const render_graph_transient_texture_update& update) noexcept {
    const granit_texture_desc native_desc{
        .struct_size = GRANIT_TEXTURE_DESC_VERSION_1_SIZE,
        .dimension = static_cast<granit_texture_dimension>(update.desc.dimension),
        .format = static_cast<granit_texture_format>(update.desc.format),
        .usage = static_cast<granit_texture_usage>(update.desc.usage),
        .memory_location = static_cast<granit_memory_location>(update.desc.location),
        .width = update.desc.width,
        .height = update.desc.height,
        .depth = update.desc.depth,
        .mip_levels = update.desc.mip_levels,
        .array_layers = update.desc.array_layers,
        .sample_count = static_cast<granit_sample_count>(update.desc.samples),
        .reserved = 0,
    };
    const granit_render_graph_transient_texture_update native_update{
        .struct_size = GRANIT_RENDER_GRAPH_TRANSIENT_TEXTURE_UPDATE_VERSION_1_SIZE,
        .resource = update.resource,
        .desc = native_desc,
        .reserved = 0,
    };
    return from_native(granit_render_graph_update_transient_texture(handle_, &native_update));
  }
  [[nodiscard]] result add_pass(const render_graph_pass_desc& desc,
                                render_graph_record_callback callback, void* user_data,
                                render_graph_pass& pass) noexcept {
    try {
      const auto native_accesses = make_native_accesses(desc.accesses);
      const granit_render_graph_pass_desc native_desc{
          .struct_size = GRANIT_RENDER_GRAPH_PASS_DESC_VERSION_1_SIZE,
          .flags = desc.side_effect ? GRANIT_RENDER_GRAPH_PASS_SIDE_EFFECT_BIT : 0U,
          .name = desc.name.data(),
          .name_length = static_cast<std::uint32_t>(desc.name.size()),
          .accesses = native_accesses.data(),
          .access_count = static_cast<std::uint32_t>(native_accesses.size()),
          .reserved = 0,
      };
      return from_native(
          granit_render_graph_add_pass(handle_, &native_desc, callback, user_data, &pass));
    } catch (const std::bad_alloc&) {
      return result::out_of_memory;
    }
  }
  [[nodiscard]] result add_dependency(render_graph_pass before, render_graph_pass after) noexcept {
    return from_native(granit_render_graph_add_dependency(handle_, before, after));
  }
  [[nodiscard]] result compile() noexcept {
    return from_native(granit_render_graph_compile(handle_));
  }
  [[nodiscard]] result execute(granit_renderer renderer, granit_frame frame,
                               std::uint32_t frame_slot,
                               render_graph_execution_result& output) noexcept {
    granit_render_graph_execution_result native_output = GRANIT_RENDER_GRAPH_EXECUTION_RESULT_INIT;
    const auto value =
        granit_render_graph_execute(handle_, renderer, frame, frame_slot, &native_output);
    output.status = from_native(native_output.result);
    output.phase = native_output.phase;
    output.graph_error = native_output.graph_error;
    output.error_pass = native_output.error_pass;
    output.error_resource = native_output.error_resource;
    output.error_pass_name = {native_output.error_pass_name, native_output.error_pass_name_length};
    output.error_resource_name = {native_output.error_resource_name,
                                  native_output.error_resource_name_length};
    output.recorder = native_output.recorder;
    return from_native(value);
  }
  [[nodiscard]] result reset() noexcept {
    if (handle_ == GRANIT_NULL_HANDLE)
      return result::success;
    const auto value = granit_render_graph_destroy(handle_);
    if (value == GRANIT_SUCCESS)
      handle_ = GRANIT_NULL_HANDLE;
    return from_native(value);
  }

private:
  [[nodiscard]] static std::vector<granit_render_graph_access_desc>
  make_native_accesses(std::span<const render_graph_access_desc> accesses) {
    std::vector<granit_render_graph_access_desc> native;
    native.reserve(accesses.size());
    for (const auto& access : accesses) {
      native.push_back({GRANIT_RENDER_GRAPH_ACCESS_DESC_VERSION_1_SIZE, access.resource,
                        static_cast<std::uint32_t>(access.access),
                        static_cast<std::uint32_t>(access.usage), 0});
    }
    return native;
  }

  render_graph_native_handle handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit::render_graph

#endif
