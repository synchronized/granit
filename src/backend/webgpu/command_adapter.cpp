// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/command_adapter.h"

#include <new>
#include <utility>
#include <vector>

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
  std::uint32_t first_vertex_buffer_{};
  std::vector<granit_backend_plugin_vertex_buffer_binding> vertex_buffers_;
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

granit_result webgpu_command_adapter::draw(backend_command_recorder_resource& resource,
                                           granit_backend_plugin_texture_view target,
                                           granit_backend_plugin_render_pipeline pipeline,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      target == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_draw(
      context_->instance, recorder->recorder_, target, pipeline, 0, recorder->first_vertex_buffer_,
      recorder->vertex_buffers_, vertex_count, instance_count, first_vertex, first_instance);
}

granit_result webgpu_command_adapter::bind_vertex_buffers(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const granit_backend_plugin_vertex_buffer_binding> bindings) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_backend_plugin_vertex_buffer_binding> updated{bindings.begin(),
                                                                     bindings.end()};
    recorder->vertex_buffers_.swap(updated);
    recorder->first_vertex_buffer_ = first;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  return GRANIT_SUCCESS;
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
  recorder->vertex_buffers_.clear();
  recorder->first_vertex_buffer_ = 0;
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
