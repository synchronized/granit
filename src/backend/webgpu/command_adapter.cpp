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
};

webgpu_command_recorder_resource* as_recorder(backend_command_recorder_resource& resource) {
  return dynamic_cast<webgpu_command_recorder_resource*>(&resource);
}

} // namespace

webgpu_command_adapter::webgpu_command_adapter(backend_plugin_loader& loader,
                                               granit_backend_plugin_instance instance)
    : context_(std::make_shared<webgpu_command_context>(
          webgpu_command_context{&loader, instance})) {}

std::unique_ptr<backend_command_recorder_resource> webgpu_command_adapter::allocate_recorder() const {
  return std::make_unique<webgpu_command_recorder_resource>(context_);
}

granit_result webgpu_command_adapter::begin(
    backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ != 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->create_command_recorder(context_->instance, &recorder->recorder_);
}

granit_result webgpu_command_adapter::draw(backend_command_recorder_resource& resource,
                                           granit_backend_plugin_texture_view target,
                                           granit_backend_plugin_render_pipeline pipeline) const
    noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      target == 0 || pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->recorder_draw(context_->instance, recorder->recorder_, target, pipeline,
                                         0);
}

granit_result webgpu_command_adapter::end(
    backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->loader->finish_command_recorder(context_->instance, recorder->recorder_,
                                                   &recorder->command_buffer_);
}

granit_result webgpu_command_adapter::submit(
    backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->command_buffer_ == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      context_->loader->submit_command_buffer(context_->instance, recorder->command_buffer_);
  if (result == GRANIT_SUCCESS)
    recorder->command_buffer_ = 0;
  return result;
}

granit_result webgpu_command_adapter::reset(
    backend_command_recorder_resource& resource) const noexcept {
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
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
