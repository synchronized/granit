// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/renderer_state.h"

#include <utility>
#include <vector>

namespace granit::detail {

namespace {

class webgpu_command_recorder_resource final : public backend_command_recorder_resource {
public:
  explicit webgpu_command_recorder_resource(std::shared_ptr<webgpu_command_owner> context)
      : command_owner_(std::move(context)) {}

  ~webgpu_command_recorder_resource() override {
    if (command_buffer_ != 0) {
      static_cast<void>(command_owner_->context->destroy_command_buffer(command_owner_->instance,
                                                                        command_buffer_));
    }
    if (recorder_ != 0) {
      static_cast<void>(
          command_owner_->context->destroy_command_recorder(command_owner_->instance, recorder_));
    }
  }

  std::shared_ptr<webgpu_command_owner> command_owner_;
  webgpu_command_recorder recorder_{};
  webgpu_command_buffer command_buffer_{};
  bool compute_open_{};
  bool render_open_{};
  webgpu_render_pipeline pipeline_{};
  struct group_binding {
    webgpu_pipeline_layout layout{};
    std::uint32_t first{};
    std::vector<webgpu_bind_group> groups;
    std::vector<std::uint32_t> dynamic_offsets;
  };
  struct vertex_binding {
    std::uint32_t first{};
    std::vector<webgpu_vertex_buffer_binding> bindings;
  };
  std::vector<group_binding> groups_;
  std::vector<vertex_binding> vertex_buffers_;
  webgpu_buffer index_buffer_{};
  std::uint64_t index_offset_{};
  webgpu_index_format index_format_{};
  std::uint32_t viewport_first_{};
  std::vector<webgpu_viewport> viewports_;
  std::uint32_t scissor_first_{};
  std::vector<webgpu_scissor> scissors_;
};

webgpu_command_recorder_resource* as_recorder(backend_command_recorder_resource& resource) {
  return dynamic_cast<webgpu_command_recorder_resource*>(&resource);
}

granit_result end_compute_if_open(const webgpu_command_owner& context,
                                  webgpu_command_recorder_resource& recorder) noexcept {
  if (!recorder.compute_open_)
    return GRANIT_SUCCESS;
  const auto result = context.context->recorder_end_compute(context.instance, recorder.recorder_);
  if (result == GRANIT_SUCCESS)
    recorder.compute_open_ = false;
  return result;
}

} // namespace

std::unique_ptr<backend_command_recorder_resource>
webgpu_renderer_state::command_allocate_recorder() {
  return std::make_unique<webgpu_command_recorder_resource>(command_owner_);
}

granit_result
webgpu_renderer_state::command_begin(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ != 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return command_owner_->context->create_command_recorder(command_owner_->instance,
                                                          &recorder->recorder_);
}

granit_result webgpu_renderer_state::command_begin_rendering(
    backend_command_recorder_resource& resource, webgpu_texture_view target,
    webgpu_texture_view resolve_target, webgpu_load_operation load, webgpu_store_operation store,
    const float clear[4], webgpu_texture_view depth_target, webgpu_load_operation depth_load,
    webgpu_store_operation depth_store, float clear_depth) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      (target == 0 && depth_target == 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  auto result = command_owner_->context->recorder_begin_rendering(
      command_owner_->instance, recorder->recorder_, target, load, store, clear, resolve_target,
      depth_target, depth_load, depth_store, clear_depth);
  if (result != GRANIT_SUCCESS)
    return result;
  recorder->render_open_ = true;
  const auto replay = [&](granit_result next) {
    if (result == GRANIT_SUCCESS)
      result = next;
  };
  if (recorder->pipeline_ != 0) {
    replay(command_owner_->context->recorder_bind_pipeline(
        command_owner_->instance, recorder->recorder_, recorder->pipeline_));
  }
  for (const auto& binding : recorder->groups_) {
    replay(command_owner_->context->recorder_bind_graphics_groups(
        command_owner_->instance, recorder->recorder_, binding.layout, binding.first,
        binding.groups, binding.dynamic_offsets));
  }
  for (const auto& binding : recorder->vertex_buffers_) {
    replay(command_owner_->context->recorder_bind_vertex_buffers(
        command_owner_->instance, recorder->recorder_, binding.first, binding.bindings));
  }
  if (recorder->index_buffer_ != 0) {
    replay(command_owner_->context->recorder_bind_index_buffer(
        command_owner_->instance, recorder->recorder_, recorder->index_buffer_,
        recorder->index_offset_, recorder->index_format_));
  }
  if (!recorder->viewports_.empty()) {
    replay(command_owner_->context->recorder_set_viewports(
        command_owner_->instance, recorder->recorder_, recorder->viewport_first_,
        recorder->viewports_));
  }
  if (!recorder->scissors_.empty()) {
    replay(command_owner_->context->recorder_set_scissors(
        command_owner_->instance, recorder->recorder_, recorder->scissor_first_,
        recorder->scissors_));
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(command_owner_->context->recorder_end_rendering(command_owner_->instance,
                                                                      recorder->recorder_));
    recorder->render_open_ = false;
  }
  return result;
}

granit_result
webgpu_renderer_state::command_bind_pipeline(backend_command_recorder_resource& resource,
                                             webgpu_render_pipeline pipeline) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->render_open_) {
    const auto result = command_owner_->context->recorder_bind_pipeline(
        command_owner_->instance, recorder->recorder_, pipeline);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  recorder->pipeline_ = pipeline;
  return GRANIT_SUCCESS;
}

granit_result webgpu_renderer_state::command_bind_graphics_groups(
    backend_command_recorder_resource& resource, webgpu_pipeline_layout layout,
    std::uint32_t first_group, std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      layout == 0 || groups.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    webgpu_command_recorder_resource::group_binding binding{
        layout,
        first_group,
        {groups.begin(), groups.end()},
        {dynamic_offsets.begin(), dynamic_offsets.end()}};
    if (recorder->render_open_) {
      const auto result = command_owner_->context->recorder_bind_graphics_groups(
          command_owner_->instance, recorder->recorder_, layout, first_group, groups,
          dynamic_offsets);
      if (result != GRANIT_SUCCESS)
        return result;
    }
    recorder->groups_.push_back(std::move(binding));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_renderer_state::command_begin_compute(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      recorder->compute_open_ || recorder->render_open_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command_owner_->context->recorder_begin_compute(command_owner_->instance,
                                                                      recorder->recorder_);
  if (result == GRANIT_SUCCESS)
    recorder->compute_open_ = true;
  return result;
}

granit_result
webgpu_renderer_state::command_bind_compute_pipeline(backend_command_recorder_resource& resource,
                                                     webgpu_compute_pipeline pipeline) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (!recorder->compute_open_) {
    const auto result = command_owner_->context->recorder_begin_compute(command_owner_->instance,
                                                                        recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = true;
  }
  return command_owner_->context->recorder_bind_compute_pipeline(command_owner_->instance,
                                                                 recorder->recorder_, pipeline);
}

granit_result webgpu_renderer_state::command_bind_compute_groups(
    backend_command_recorder_resource& resource, webgpu_pipeline_layout layout,
    std::uint32_t first_group, std::span<const webgpu_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                             : command_owner_->context->recorder_bind_compute_groups(
                                   command_owner_->instance, recorder->recorder_, layout,
                                   first_group, groups, dynamic_offsets);
}

granit_result webgpu_renderer_state::command_dispatch(backend_command_recorder_resource& resource,
                                                      std::uint32_t x, std::uint32_t y,
                                                      std::uint32_t z) noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                             : command_owner_->context->recorder_dispatch(
                                   command_owner_->instance, recorder->recorder_, x, y, z);
}

granit_result
webgpu_renderer_state::command_end_compute(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || !recorder->compute_open_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return end_compute_if_open(*command_owner_, *recorder);
}

granit_result webgpu_renderer_state::command_bind_vertex_buffers(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const webgpu_vertex_buffer_binding> bindings) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    webgpu_command_recorder_resource::vertex_binding binding{first,
                                                             {bindings.begin(), bindings.end()}};
    if (recorder->render_open_) {
      const auto result = command_owner_->context->recorder_bind_vertex_buffers(
          command_owner_->instance, recorder->recorder_, first, bindings);
      if (result != GRANIT_SUCCESS)
        return result;
    }
    recorder->vertex_buffers_.push_back(std::move(binding));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_renderer_state::command_bind_index_buffer(backend_command_recorder_resource& resource,
                                                 webgpu_buffer buffer, std::uint64_t offset,
                                                 webgpu_index_format format) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      buffer == 0 ||
      (format != GRANIT_WEBGPU_INDEX_FORMAT_UINT16 && format != GRANIT_WEBGPU_INDEX_FORMAT_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->render_open_) {
    const auto result = command_owner_->context->recorder_bind_index_buffer(
        command_owner_->instance, recorder->recorder_, buffer, offset, format);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  recorder->index_buffer_ = buffer;
  recorder->index_offset_ = offset;
  recorder->index_format_ = format;
  return GRANIT_SUCCESS;
}

granit_result
webgpu_renderer_state::command_set_viewports(backend_command_recorder_resource& resource,
                                             std::uint32_t first,
                                             std::span<const webgpu_viewport> viewports) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || viewports.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_viewport> pending{viewports.begin(), viewports.end()};
    if (recorder->render_open_) {
      const auto result = command_owner_->context->recorder_set_viewports(
          command_owner_->instance, recorder->recorder_, first, viewports);
      if (result != GRANIT_SUCCESS)
        return result;
    }
    recorder->viewport_first_ = first;
    recorder->viewports_ = std::move(pending);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
webgpu_renderer_state::command_set_scissors(backend_command_recorder_resource& resource,
                                            std::uint32_t first,
                                            std::span<const webgpu_scissor> scissors) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || scissors.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<webgpu_scissor> pending{scissors.begin(), scissors.end()};
    if (recorder->render_open_) {
      const auto result = command_owner_->context->recorder_set_scissors(
          command_owner_->instance, recorder->recorder_, first, scissors);
      if (result != GRANIT_SUCCESS)
        return result;
    }
    recorder->scissor_first_ = first;
    recorder->scissors_ = std::move(pending);
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result webgpu_renderer_state::command_copy_texture_to_buffer(
    backend_command_recorder_resource& resource, webgpu_texture texture, webgpu_buffer buffer,
    std::uint32_t width, std::uint32_t height, std::uint32_t bytes_per_row) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || texture == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_copy_texture_to_buffer(
      command_owner_->instance, recorder->recorder_, texture, buffer, width, height, bytes_per_row);
}

granit_result webgpu_renderer_state::command_copy_buffer(
    backend_command_recorder_resource& resource, webgpu_buffer source, webgpu_buffer destination,
    std::span<const webgpu_buffer_copy_region> regions) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0 ||
      regions.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_copy_buffer(
      command_owner_->instance, recorder->recorder_, source, destination, regions);
}

granit_result webgpu_renderer_state::command_copy_buffer_to_texture(
    backend_command_recorder_resource& resource, webgpu_buffer source, webgpu_texture destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_copy_buffer_to_texture_v2(
      command_owner_->instance, recorder->recorder_, source, destination, region);
}

granit_result webgpu_renderer_state::command_copy_texture_to_buffer(
    backend_command_recorder_resource& resource, webgpu_texture source, webgpu_buffer destination,
    const webgpu_texture_buffer_copy& region) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_copy_texture_to_buffer_v2(
      command_owner_->instance, recorder->recorder_, source, destination, region);
}

granit_result
webgpu_renderer_state::command_copy_texture(backend_command_recorder_resource& resource,
                                            webgpu_texture source, webgpu_texture destination,
                                            const webgpu_texture_copy_region& region) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_copy_texture(
      command_owner_->instance, recorder->recorder_, source, destination, region);
}

granit_result
webgpu_renderer_state::command_fill_buffer(backend_command_recorder_resource& resource,
                                           webgpu_buffer buffer, std::uint64_t offset,
                                           std::uint64_t size, std::uint32_t value) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || buffer == 0 || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_fill_buffer(
      command_owner_->instance, recorder->recorder_, buffer, offset, size, value);
}

granit_result
webgpu_renderer_state::command_generate_mipmaps(backend_command_recorder_resource& resource,
                                                webgpu_texture texture,
                                                const webgpu_texture_mipmap_range& range) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*command_owner_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return command_owner_->context->recorder_generate_mipmaps(command_owner_->instance,
                                                            recorder->recorder_, texture, range);
}

granit_result webgpu_renderer_state::command_draw(backend_command_recorder_resource& resource,
                                                  std::uint32_t vertex_count,
                                                  std::uint32_t instance_count,
                                                  std::uint32_t first_vertex,
                                                  std::uint32_t first_instance) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return command_owner_->context->recorder_draw_vertices(
      command_owner_->instance, recorder->recorder_, vertex_count, instance_count, first_vertex,
      first_instance);
}

granit_result
webgpu_renderer_state::command_draw_indexed(backend_command_recorder_resource& resource,
                                            std::uint32_t index_count, std::uint32_t instance_count,
                                            std::uint32_t first_index, std::int32_t vertex_offset,
                                            std::uint32_t first_instance) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return command_owner_->context->recorder_draw_indices(
      command_owner_->instance, recorder->recorder_, index_count, instance_count, first_index,
      vertex_offset, first_instance);
}

granit_result
webgpu_renderer_state::command_end_rendering(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command_owner_->context->recorder_end_rendering(command_owner_->instance,
                                                                      recorder->recorder_);
  if (result == GRANIT_SUCCESS)
    recorder->render_open_ = false;
  return result;
}

bool webgpu_renderer_state::command_is_recording(
    backend_command_recorder_resource& resource) noexcept {
  const auto* recorder = as_recorder(resource);
  return recorder != nullptr && recorder->recorder_ != 0 && recorder->command_buffer_ == 0;
}

granit_result
webgpu_renderer_state::command_end(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->compute_open_) {
    const auto result = command_owner_->context->recorder_end_compute(command_owner_->instance,
                                                                      recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = false;
  }
  return command_owner_->context->finish_command_recorder(
      command_owner_->instance, recorder->recorder_, &recorder->command_buffer_);
}

granit_result
webgpu_renderer_state::command_submit(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->command_buffer_ == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result = command_owner_->context->submit_command_buffer(command_owner_->instance,
                                                                     recorder->command_buffer_);
  if (result == GRANIT_SUCCESS)
    recorder->command_buffer_ = 0;
  return result;
}

granit_result
webgpu_renderer_state::command_reset(backend_command_recorder_resource& resource) noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->command_buffer_ != 0) {
    const auto result = command_owner_->context->destroy_command_buffer(command_owner_->instance,
                                                                        recorder->command_buffer_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->command_buffer_ = 0;
  }
  if (recorder->recorder_ != 0) {
    const auto result = command_owner_->context->destroy_command_recorder(command_owner_->instance,
                                                                          recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->recorder_ = 0;
  }
  recorder->compute_open_ = false;
  recorder->render_open_ = false;
  recorder->pipeline_ = 0;
  recorder->groups_.clear();
  recorder->vertex_buffers_.clear();
  recorder->index_buffer_ = 0;
  recorder->index_offset_ = 0;
  recorder->index_format_ = 0;
  recorder->viewports_.clear();
  recorder->scissors_.clear();
  return GRANIT_SUCCESS;
}

webgpu_command_recorder webgpu_renderer_state::command_native_recorder(
    backend_command_recorder_resource& resource) noexcept {
  const auto* recorder = as_recorder(resource);
  return recorder != nullptr && recorder->command_buffer_ == 0 ? recorder->recorder_ : 0;
}

} // namespace granit::detail
