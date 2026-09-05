// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/command_adapter.h"

#include <utility>
#include <vector>

namespace granit::detail {

struct webgpu_command_context {
  webgpu_provider_dispatch* provider{};
  granit_webgpu_provider_instance instance{};
};

namespace {

class webgpu_command_recorder_resource final : public backend_command_recorder_resource {
public:
  explicit webgpu_command_recorder_resource(std::shared_ptr<webgpu_command_context> context)
      : context_(std::move(context)) {}

  ~webgpu_command_recorder_resource() override {
    if (command_buffer_ != 0) {
      static_cast<void>(
          context_->provider->destroy_command_buffer(context_->instance, command_buffer_));
    }
    if (recorder_ != 0) {
      static_cast<void>(
          context_->provider->destroy_command_recorder(context_->instance, recorder_));
    }
  }

  std::shared_ptr<webgpu_command_context> context_;
  granit_webgpu_provider_command_recorder recorder_{};
  granit_webgpu_provider_command_buffer command_buffer_{};
  bool compute_open_{};
  bool render_open_{};
  granit_webgpu_provider_render_pipeline pipeline_{};
  struct group_binding {
    granit_webgpu_provider_pipeline_layout layout{};
    std::uint32_t first{};
    std::vector<granit_webgpu_provider_bind_group> groups;
    std::vector<std::uint32_t> dynamic_offsets;
  };
  struct vertex_binding {
    std::uint32_t first{};
    std::vector<granit_webgpu_provider_vertex_buffer_binding> bindings;
  };
  std::vector<group_binding> groups_;
  std::vector<vertex_binding> vertex_buffers_;
  granit_webgpu_provider_buffer index_buffer_{};
  std::uint64_t index_offset_{};
  granit_webgpu_provider_index_format index_format_{};
  std::uint32_t viewport_first_{};
  std::vector<granit_webgpu_provider_viewport> viewports_;
  std::uint32_t scissor_first_{};
  std::vector<granit_webgpu_provider_scissor> scissors_;
};

webgpu_command_recorder_resource* as_recorder(backend_command_recorder_resource& resource) {
  return dynamic_cast<webgpu_command_recorder_resource*>(&resource);
}

granit_result end_compute_if_open(const webgpu_command_context& context,
                                  webgpu_command_recorder_resource& recorder) noexcept {
  if (!recorder.compute_open_)
    return GRANIT_SUCCESS;
  const auto result = context.provider->recorder_end_compute(context.instance, recorder.recorder_);
  if (result == GRANIT_SUCCESS)
    recorder.compute_open_ = false;
  return result;
}

} // namespace

webgpu_command_adapter::webgpu_command_adapter(webgpu_provider_dispatch& provider,
                                               granit_webgpu_provider_instance instance)
    : context_(
          std::make_shared<webgpu_command_context>(webgpu_command_context{&provider, instance})) {}

std::unique_ptr<backend_command_recorder_resource>
webgpu_command_adapter::allocate_recorder() const {
  return std::make_unique<webgpu_command_recorder_resource>(context_);
}

granit_result
webgpu_command_adapter::begin(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ != 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->provider->create_command_recorder(context_->instance, &recorder->recorder_);
}

granit_result webgpu_command_adapter::begin_rendering(
    backend_command_recorder_resource& resource, granit_webgpu_provider_texture_view target,
    granit_webgpu_provider_texture_view resolve_target, granit_webgpu_provider_load_operation load,
    granit_webgpu_provider_store_operation store, const float clear[4],
    granit_webgpu_provider_texture_view depth_target,
    granit_webgpu_provider_load_operation depth_load,
    granit_webgpu_provider_store_operation depth_store, float clear_depth) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      (target == 0 && depth_target == 0))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  auto result = context_->provider->recorder_begin_rendering(
      context_->instance, recorder->recorder_, target, load, store, clear, resolve_target,
      depth_target, depth_load, depth_store, clear_depth);
  if (result != GRANIT_SUCCESS)
    return result;
  recorder->render_open_ = true;
  const auto replay = [&](granit_result next) {
    if (result == GRANIT_SUCCESS)
      result = next;
  };
  if (recorder->pipeline_ != 0) {
    replay(context_->provider->recorder_bind_pipeline(context_->instance, recorder->recorder_,
                                                      recorder->pipeline_));
  }
  for (const auto& binding : recorder->groups_) {
    replay(context_->provider->recorder_bind_graphics_groups(
        context_->instance, recorder->recorder_, binding.layout, binding.first, binding.groups,
        binding.dynamic_offsets));
  }
  for (const auto& binding : recorder->vertex_buffers_) {
    replay(context_->provider->recorder_bind_vertex_buffers(context_->instance, recorder->recorder_,
                                                            binding.first, binding.bindings));
  }
  if (recorder->index_buffer_ != 0) {
    replay(context_->provider->recorder_bind_index_buffer(
        context_->instance, recorder->recorder_, recorder->index_buffer_, recorder->index_offset_,
        recorder->index_format_));
  }
  if (!recorder->viewports_.empty()) {
    replay(context_->provider->recorder_set_viewports(
        context_->instance, recorder->recorder_, recorder->viewport_first_, recorder->viewports_));
  }
  if (!recorder->scissors_.empty()) {
    replay(context_->provider->recorder_set_scissors(
        context_->instance, recorder->recorder_, recorder->scissor_first_, recorder->scissors_));
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(
        context_->provider->recorder_end_rendering(context_->instance, recorder->recorder_));
    recorder->render_open_ = false;
  }
  return result;
}

granit_result webgpu_command_adapter::bind_pipeline(
    backend_command_recorder_resource& resource,
    granit_webgpu_provider_render_pipeline pipeline) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      pipeline == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->render_open_) {
    const auto result = context_->provider->recorder_bind_pipeline(context_->instance,
                                                                   recorder->recorder_, pipeline);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  recorder->pipeline_ = pipeline;
  return GRANIT_SUCCESS;
}

granit_result webgpu_command_adapter::bind_graphics_groups(
    backend_command_recorder_resource& resource, granit_webgpu_provider_pipeline_layout layout,
    std::uint32_t first_group, std::span<const granit_webgpu_provider_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) const noexcept {
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
      const auto result = context_->provider->recorder_bind_graphics_groups(
          context_->instance, recorder->recorder_, layout, first_group, groups, dynamic_offsets);
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
webgpu_command_adapter::begin_compute(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      recorder->compute_open_ || recorder->render_open_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      context_->provider->recorder_begin_compute(context_->instance, recorder->recorder_);
  if (result == GRANIT_SUCCESS)
    recorder->compute_open_ = true;
  return result;
}

granit_result webgpu_command_adapter::bind_compute_pipeline(
    backend_command_recorder_resource& resource,
    granit_webgpu_provider_compute_pipeline pipeline) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (!recorder->compute_open_) {
    const auto result =
        context_->provider->recorder_begin_compute(context_->instance, recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = true;
  }
  return context_->provider->recorder_bind_compute_pipeline(context_->instance, recorder->recorder_,
                                                            pipeline);
}

granit_result webgpu_command_adapter::bind_compute_groups(
    backend_command_recorder_resource& resource, granit_webgpu_provider_pipeline_layout layout,
    std::uint32_t first_group, std::span<const granit_webgpu_provider_bind_group> groups,
    std::span<const std::uint32_t> dynamic_offsets) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                             : context_->provider->recorder_bind_compute_groups(
                                   context_->instance, recorder->recorder_, layout, first_group,
                                   groups, dynamic_offsets);
}

granit_result webgpu_command_adapter::dispatch(backend_command_recorder_resource& resource,
                                               std::uint32_t x, std::uint32_t y,
                                               std::uint32_t z) const noexcept {
  auto* recorder = as_recorder(resource);
  return recorder == nullptr ? GRANIT_ERROR_INVALID_ARGUMENT
                             : context_->provider->recorder_dispatch(context_->instance,
                                                                     recorder->recorder_, x, y, z);
}

granit_result
webgpu_command_adapter::end_compute(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || !recorder->compute_open_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return end_compute_if_open(*context_, *recorder);
}

granit_result webgpu_command_adapter::bind_vertex_buffers(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const granit_webgpu_provider_vertex_buffer_binding> bindings) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      bindings.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    webgpu_command_recorder_resource::vertex_binding binding{first,
                                                             {bindings.begin(), bindings.end()}};
    if (recorder->render_open_) {
      const auto result = context_->provider->recorder_bind_vertex_buffers(
          context_->instance, recorder->recorder_, first, bindings);
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

granit_result webgpu_command_adapter::bind_index_buffer(
    backend_command_recorder_resource& resource, granit_webgpu_provider_buffer buffer,
    std::uint64_t offset, granit_webgpu_provider_index_format format) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0 ||
      buffer == 0 ||
      (format != GRANIT_WEBGPU_PROVIDER_INDEX_FORMAT_UINT16 &&
       format != GRANIT_WEBGPU_PROVIDER_INDEX_FORMAT_UINT32))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (recorder->render_open_) {
    const auto result = context_->provider->recorder_bind_index_buffer(
        context_->instance, recorder->recorder_, buffer, offset, format);
    if (result != GRANIT_SUCCESS)
      return result;
  }
  recorder->index_buffer_ = buffer;
  recorder->index_offset_ = offset;
  recorder->index_format_ = format;
  return GRANIT_SUCCESS;
}

granit_result webgpu_command_adapter::set_viewports(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const granit_webgpu_provider_viewport> viewports) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || viewports.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_webgpu_provider_viewport> pending{viewports.begin(), viewports.end()};
    if (recorder->render_open_) {
      const auto result = context_->provider->recorder_set_viewports(
          context_->instance, recorder->recorder_, first, viewports);
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

granit_result webgpu_command_adapter::set_scissors(
    backend_command_recorder_resource& resource, std::uint32_t first,
    std::span<const granit_webgpu_provider_scissor> scissors) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || scissors.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  try {
    std::vector<granit_webgpu_provider_scissor> pending{scissors.begin(), scissors.end()};
    if (recorder->render_open_) {
      const auto result = context_->provider->recorder_set_scissors(
          context_->instance, recorder->recorder_, first, scissors);
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

granit_result webgpu_command_adapter::copy_texture_to_buffer(
    backend_command_recorder_resource& resource, granit_webgpu_provider_texture texture,
    granit_webgpu_provider_buffer buffer, std::uint32_t width, std::uint32_t height,
    std::uint32_t bytes_per_row) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || texture == 0 || buffer == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_copy_texture_to_buffer(
      context_->instance, recorder->recorder_, texture, buffer, width, height, bytes_per_row);
}

granit_result webgpu_command_adapter::copy_buffer(
    backend_command_recorder_resource& resource, granit_webgpu_provider_buffer source,
    granit_webgpu_provider_buffer destination,
    std::span<const granit_webgpu_provider_buffer_copy_region> regions) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0 ||
      regions.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_copy_buffer(context_->instance, recorder->recorder_, source,
                                                  destination, regions);
}

granit_result webgpu_command_adapter::copy_buffer_to_texture(
    backend_command_recorder_resource& resource, granit_webgpu_provider_buffer source,
    granit_webgpu_provider_texture destination,
    const granit_webgpu_provider_texture_buffer_copy& region) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_copy_buffer_to_texture_v2(
      context_->instance, recorder->recorder_, source, destination, region);
}

granit_result webgpu_command_adapter::copy_texture_to_buffer(
    backend_command_recorder_resource& resource, granit_webgpu_provider_texture source,
    granit_webgpu_provider_buffer destination,
    const granit_webgpu_provider_texture_buffer_copy& region) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_copy_texture_to_buffer_v2(
      context_->instance, recorder->recorder_, source, destination, region);
}

granit_result webgpu_command_adapter::copy_texture(
    backend_command_recorder_resource& resource, granit_webgpu_provider_texture source,
    granit_webgpu_provider_texture destination,
    const granit_webgpu_provider_texture_copy_region& region) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || source == 0 || destination == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_copy_texture(context_->instance, recorder->recorder_, source,
                                                   destination, region);
}

granit_result webgpu_command_adapter::fill_buffer(backend_command_recorder_resource& resource,
                                                  granit_webgpu_provider_buffer buffer,
                                                  std::uint64_t offset, std::uint64_t size,
                                                  std::uint32_t value) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || buffer == 0 || size == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_fill_buffer(context_->instance, recorder->recorder_, buffer,
                                                  offset, size, value);
}

granit_result webgpu_command_adapter::generate_mipmaps(
    backend_command_recorder_resource& resource, granit_webgpu_provider_texture texture,
    const granit_webgpu_provider_texture_mipmap_range& range) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->render_open_ || texture == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (const auto result = end_compute_if_open(*context_, *recorder); result != GRANIT_SUCCESS)
    return result;
  return context_->provider->recorder_generate_mipmaps(context_->instance, recorder->recorder_,
                                                       texture, range);
}

granit_result webgpu_command_adapter::draw(backend_command_recorder_resource& resource,
                                           std::uint32_t vertex_count, std::uint32_t instance_count,
                                           std::uint32_t first_vertex,
                                           std::uint32_t first_instance) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  return context_->provider->recorder_draw_vertices(context_->instance, recorder->recorder_,
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
  return context_->provider->recorder_draw_indices(context_->instance, recorder->recorder_,
                                                   index_count, instance_count, first_index,
                                                   vertex_offset, first_instance);
}

granit_result
webgpu_command_adapter::end_rendering(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->recorder_ == 0 || recorder->command_buffer_ != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      context_->provider->recorder_end_rendering(context_->instance, recorder->recorder_);
  if (result == GRANIT_SUCCESS)
    recorder->render_open_ = false;
  return result;
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
        context_->provider->recorder_end_compute(context_->instance, recorder->recorder_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->compute_open_ = false;
  }
  return context_->provider->finish_command_recorder(context_->instance, recorder->recorder_,
                                                     &recorder->command_buffer_);
}

granit_result
webgpu_command_adapter::submit(backend_command_recorder_resource& resource) const noexcept {
  auto* recorder = as_recorder(resource);
  if (recorder == nullptr || recorder->command_buffer_ == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto result =
      context_->provider->submit_command_buffer(context_->instance, recorder->command_buffer_);
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
        context_->provider->destroy_command_buffer(context_->instance, recorder->command_buffer_);
    if (result != GRANIT_SUCCESS)
      return result;
    recorder->command_buffer_ = 0;
  }
  if (recorder->recorder_ != 0) {
    const auto result =
        context_->provider->destroy_command_recorder(context_->instance, recorder->recorder_);
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

} // namespace granit::detail
