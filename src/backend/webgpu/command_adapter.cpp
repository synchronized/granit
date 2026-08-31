// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/command_adapter.h"

#include <utility>

namespace granit::detail {

struct webgpu_command_context {
  backend_plugin_loader* loader{};
  granit_backend_plugin_instance instance{};
};

namespace {

class webgpu_command_recorder_resource final : public backend_command_recorder_resource {
public:
  explicit webgpu_command_recorder_resource(std::shared_ptr<webgpu_command_context> context)
      : context_(std::move(context)) {}

  ~webgpu_command_recorder_resource() override {
    if (command_buffer_ != 0) {
      static_cast<void>(
          context_->loader->destroy_command_buffer(context_->instance, command_buffer_));
    }
    if (recorder_ != 0) {
      static_cast<void>(context_->loader->destroy_command_recorder(context_->instance, recorder_));
    }
  }

  std::shared_ptr<webgpu_command_context> context_;
  granit_backend_plugin_command_recorder recorder_{};
  granit_backend_plugin_command_buffer command_buffer_{};
  bool compute_open_{};
};

webgpu_command_recorder_resource* as_recorder(backend_command_recorder_resource& resource) {
  return dynamic_cast<webgpu_command_recorder_resource*>(&resource);
}

} // namespace

webgpu_command_adapter::webgpu_command_adapter(backend_plugin_loader& loader,
                                               granit_backend_plugin_instance instance)
    : context_(
          std::make_shared<webgpu_command_context>(webgpu_command_context{&loader, instance})) {}

std::unique_ptr<backend_command_recorder_resource>
webgpu_command_adapter::allocate_recorder() const {
  return std::make_unique<webgpu_command_recorder_resource>(context_);
}

granit_result
webgpu_command_adapter::begin(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ != 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->create_command_recorder(context_->instance, &recorder->recorder_);
}

granit_result webgpu_command_adapter::begin_rendering(backend_command_recorder_resource& resource,
                                                      granit_backend_plugin_texture_view target,
                                                      granit_backend_plugin_load_operation load,
                                                      granit_backend_plugin_store_operation store,
                                                      const float clear[4]) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      target == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_begin_rendering(context_->instance, recorder->recorder_, target,
                                                    load, store, clear);
}

granit_result webgpu_command_adapter::bind_pipeline(
    backend_command_recorder_resource& resource,
    granit_backend_plugin_render_pipeline pipeline) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_bind_pipeline(context_->instance, recorder->recorder_,
                                                  pipeline);
}

granit_result webgpu_command_adapter::bind_graphics_groups(
    backend_command_recorder_resource& resource, granit_backend_plugin_pipeline_layout layout,
    std::uint32_t first_group, std::span<const granit_backend_plugin_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      layout == 0 || groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_bind_graphics_groups(
      context_->instance, recorder->recorder_, layout, first_group, groups, dynamic_offsets);
}

granit_result
webgpu_command_adapter::begin_compute(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr
             ? GRANIT_ERROR_INVALID_ARGUMENT
             : context_->loader->recorder_begin_compute(context_->instance, recorder->recorder_);
}

granit_result webgpu_command_adapter::bind_compute_pipeline(
    backend_command_recorder_resource& resource,
    granit_backend_plugin_compute_pipeline pipeline) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (!recorder->compute_open_) {
    const auto result =
        context_->loader->recorder_begin_compute(context_->instance, recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = true;
  }
  return context_->loader->recorder_bind_compute_pipeline(context_->instance, recorder->recorder_,
                                                          pipeline);
}

granit_result webgpu_command_adapter::bind_compute_groups(
    backend_command_recorder_resource& resource, granit_backend_plugin_pipeline_layout layout,
    std::uint32_t first_group, std::span<const granit_backend_plugin_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr
             ? GRANIT_ERROR_INVALID_ARGUMENT
             : context_->loader->recorder_bind_compute_groups(context_->instance,
                                                              recorder->recorder_, layout,
                                                              first_group, groups, dynamic_offsets);
}

granit_result webgpu_command_adapter::dispatch(backend_command_recorder_resource& resource,
                                               std::uint32_t x, std::uint32_t y,
                                               std::uint32_t z) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                             : context_->loader->recorder_dispatch(context_->instance,
                                                                   recorder->recorder_, x, y, z);
}

granit_result
webgpu_command_adapter::end_compute(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr
             ? GRANIT_ERROR_INVALID_ARGUMENT
             : context_->loader->recorder_end_compute(context_->instance, recorder->recorder_);
}

granit_result webgpu_command_adapter::bind_vertex_buffers(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const granit_backend_plugin_vertex_buffer_binding> bindings) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_bind_vertex_buffers(context_->instance, recorder->recorder_,
                                                        first, bindings);
}

granit_result webgpu_command_adapter::bind_index_buffer(
    backend_command_recorder_resource& resource, granit_backend_plugin_buffer buffer,
    std::uint64_t offset, granit_backend_plugin_index_format format) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      buffer == 0 ||
      (format != GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT16 &&
       format != GRANIT_BACKEND_PLUGIN_INDEX_FORMAT_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_bind_index_buffer(context_->instance, recorder->recorder_,
                                                      buffer, offset, format);
}

granit_result webgpu_command_adapter::draw(backend_command_recorder_resource& resource,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_draw_vertices(context_->instance, recorder->recorder_,
                                                  vertex_count, instance_count, first_vertex,
                                                  first_instance);
}

granit_result webgpu_command_adapter::draw_indexed(backend_command_recorder_resource& resource,
                                                   std::uint32_t index_count,
                                                   std::uint32_t instance_count,
                                                   std::uint32_t first_index,
                                                   std::int32_t vertex_offset,
                                                   std::uint32_t first_instance) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_draw_indices(context_->instance, recorder->recorder_,
                                                 index_count, instance_count, first_index,
                                                 vertex_offset, first_instance);
}

granit_result
webgpu_command_adapter::end_rendering(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_end_rendering(context_->instance, recorder->recorder_);
}

bool webgpu_command_adapter::is_recording(
    backend_command_recorder_resource& resource) const noexcept {
  const auto* recorder = as_recorder(resource);
  return recorder != nullptr && recorder->recorder_ != 0 && recorder->command_buffer_ == 0;
}

granit_result
webgpu_command_adapter::end(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->compute_open_) {
    const auto result =
        context_->loader->recorder_end_compute(context_->instance, recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = false;
  }
  return context_->loader->finish_command_recorder(context_->instance, recorder->recorder_,
                                                   &recorder->command_buffer_);
}

granit_result
webgpu_command_adapter::submit(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->command_buffer_ == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      context_->loader->submit_command_buffer(context_->instance, recorder->command_buffer_);
  if (result == GRANIT_SUCCESS)
    recorder->command_buffer_ = 0;
  return result;
}

granit_result
webgpu_command_adapter::reset(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->command_buffer_ != 0) {
    const auto result =
        context_->loader->destroy_command_buffer(context_->instance, recorder->command_buffer_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->command_buffer_ = 0;
  }
  if (recorder->recorder_ != 0) {
    const auto result =
        context_->loader->destroy_command_recorder(context_->instance, recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->recorder_ = 0;
  }
  recorder->compute_open_ = false;
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
