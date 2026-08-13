// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_template_gpu.h"

#include <granit/renderer/shader.h>

#include <algorithm>
#include <array>
#include <new>

namespace granit::material {
namespace {

bool same_request(const material_pipeline_request& left,
                  const material_pipeline_request& right) noexcept {
  return left.pass == right.pass && left.variant == right.variant &&
         left.color_format == right.color_format &&
         left.depth_stencil_format == right.depth_stencil_format &&
         left.sample_count == right.sample_count;
}

granit_shader_stage native_stage(package_shader_stage stage) noexcept {
  return stage == package_shader_stage::vertex ? GRANIT_SHADER_STAGE_VERTEX
                                               : GRANIT_SHADER_STAGE_FRAGMENT;
}

granit_result create_shader(granit_renderer renderer, const material_shader_code& source,
                            granit_shader& shader) noexcept {
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  desc.stage = native_stage(source.stage);
  desc.code = source.spirv.data();
  desc.code_size = source.spirv.size() * sizeof(std::uint32_t);
  desc.entry_point = source.entry_point.data();
  desc.entry_point_length = static_cast<std::uint32_t>(source.entry_point.size());
  return granit_shader_create(renderer, &desc, &shader);
}

} // namespace

material_template_gpu::~material_template_gpu() { static_cast<void>(reset()); }

granit_result
material_template_gpu::initialize(granit_renderer renderer, const material_package& package,
                                  std::span<const granit_bind_group_layout> additional_layouts) {
  std::lock_guard lock{mutex_};
  if (renderer_ != GRANIT_NULL_HANDLE || renderer == GRANIT_NULL_HANDLE ||
      package.binding_model() != package_binding_model::bind_group ||
      package.required_renderer_features() != 0 || additional_layouts.size() > 6 ||
      std::ranges::find(additional_layouts, GRANIT_NULL_HANDLE) != additional_layouts.end()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  std::vector<granit_bind_group_layout_entry> material_entries;
  try {
    material_entries.reserve(package.metadata().parameters().size() + 1U);
    if (package.metadata().constant_buffer_size() != 0) {
      material_entries.push_back(
          {0, GRANIT_BINDING_TYPE_UNIFORM_BUFFER, 1, GRANIT_SHADER_STAGE_FRAGMENT_BIT});
    }
    for (const auto& parameter : package.metadata().parameters()) {
      if (parameter.type == parameter_type::texture_view) {
        material_entries.push_back({parameter.binding, GRANIT_BINDING_TYPE_SAMPLED_TEXTURE, 1,
                                    GRANIT_SHADER_STAGE_FRAGMENT_BIT});
      } else if (parameter.type == parameter_type::sampler) {
        material_entries.push_back(
            {parameter.binding, GRANIT_BINDING_TYPE_SAMPLER, 1, GRANIT_SHADER_STAGE_FRAGMENT_BIT});
      }
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  const granit_bind_group_layout_entry frame_entry{
      0, GRANIT_BINDING_TYPE_UNIFORM_BUFFER, 1,
      GRANIT_SHADER_STAGE_VERTEX_BIT | GRANIT_SHADER_STAGE_FRAGMENT_BIT};
  granit_bind_group_layout_desc frame_desc = GRANIT_BIND_GROUP_LAYOUT_DESC_INIT;
  frame_desc.entry_count = 1;
  frame_desc.entries = &frame_entry;
  granit_bind_group_layout frame_layout = GRANIT_NULL_HANDLE;
  auto result = granit_bind_group_layout_create(renderer, &frame_desc, &frame_layout);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  granit_bind_group_layout_desc material_desc = GRANIT_BIND_GROUP_LAYOUT_DESC_INIT;
  material_desc.entry_count = static_cast<std::uint32_t>(material_entries.size());
  material_desc.entries = material_entries.data();
  granit_bind_group_layout material_layout = GRANIT_NULL_HANDLE;
  result = granit_bind_group_layout_create(renderer, &material_desc, &material_layout);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_bind_group_layout_destroy(renderer, frame_layout));
    return result;
  }
  std::vector<granit_bind_group_layout> layouts;
  try {
    layouts.reserve(2 + additional_layouts.size());
    layouts.push_back(frame_layout);
    layouts.push_back(material_layout);
    layouts.insert(layouts.end(), additional_layouts.begin(), additional_layouts.end());
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_bind_group_layout_destroy(renderer, material_layout));
    static_cast<void>(granit_bind_group_layout_destroy(renderer, frame_layout));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  granit_pipeline_layout_desc pipeline_desc = GRANIT_PIPELINE_LAYOUT_DESC_INIT;
  pipeline_desc.bind_group_layout_count = static_cast<std::uint32_t>(layouts.size());
  pipeline_desc.bind_group_layouts = layouts.data();
  granit_pipeline_layout pipeline_layout = GRANIT_NULL_HANDLE;
  result = granit_pipeline_layout_create(renderer, &pipeline_desc, &pipeline_layout);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_bind_group_layout_destroy(renderer, material_layout));
    static_cast<void>(granit_bind_group_layout_destroy(renderer, frame_layout));
    return result;
  }

  renderer_ = renderer;
  package_ = &package;
  frame_layout_ = frame_layout;
  material_layout_ = material_layout;
  pipeline_layout_ = pipeline_layout;
  return GRANIT_SUCCESS;
}

granit_result material_template_gpu::reset() noexcept {
  std::lock_guard lock{mutex_};
  granit_result first_error = GRANIT_SUCCESS;
  const auto capture = [&](granit_result result) {
    if (first_error == GRANIT_SUCCESS && result != GRANIT_SUCCESS) {
      first_error = result;
    }
  };
  for (auto& entry : cache_) {
    capture(granit_graphics_pipeline_destroy(renderer_, entry.pipeline));
    capture(granit_shader_destroy(renderer_, entry.fragment_shader));
    capture(granit_shader_destroy(renderer_, entry.vertex_shader));
  }
  if (pipeline_layout_ != GRANIT_NULL_HANDLE) {
    capture(granit_pipeline_layout_destroy(renderer_, pipeline_layout_));
  }
  if (material_layout_ != GRANIT_NULL_HANDLE) {
    capture(granit_bind_group_layout_destroy(renderer_, material_layout_));
  }
  if (frame_layout_ != GRANIT_NULL_HANDLE) {
    capture(granit_bind_group_layout_destroy(renderer_, frame_layout_));
  }
  cache_.clear();
  renderer_ = GRANIT_NULL_HANDLE;
  package_ = nullptr;
  frame_layout_ = GRANIT_NULL_HANDLE;
  material_layout_ = GRANIT_NULL_HANDLE;
  pipeline_layout_ = GRANIT_NULL_HANDLE;
  return first_error;
}

granit_result material_template_gpu::acquire_pipeline(const material_pipeline_request& request,
                                                      granit_graphics_pipeline& pipeline) {
  std::lock_guard lock{mutex_};
  pipeline = GRANIT_NULL_HANDLE;
  if (renderer_ == GRANIT_NULL_HANDLE || package_ == nullptr || request.pass == 0 ||
      request.color_format == GRANIT_TEXTURE_FORMAT_UNDEFINED) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto cached = std::ranges::find_if(
      cache_, [&](const auto& entry) { return same_request(entry.request, request); });
  if (cached != cache_.end()) {
    pipeline = cached->pipeline;
    return GRANIT_SUCCESS;
  }
  const auto* variant = package_->find(request.pass, request.variant);
  if (variant == nullptr) {
    return GRANIT_ERROR_NOT_READY;
  }
  const auto vertex = std::ranges::find(variant->shaders, package_shader_stage::vertex,
                                        &material_shader_code::stage);
  const auto fragment = std::ranges::find(variant->shaders, package_shader_stage::fragment,
                                          &material_shader_code::stage);

  cache_entry replacement{.request = request};
  auto result = create_shader(renderer_, *vertex, replacement.vertex_shader);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  result = create_shader(renderer_, *fragment, replacement.fragment_shader);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_shader_destroy(renderer_, replacement.vertex_shader));
    return result;
  }
  granit_graphics_pipeline_desc desc = GRANIT_GRAPHICS_PIPELINE_DESC_INIT;
  std::vector<std::vector<granit_vertex_attribute>> native_attributes;
  std::vector<granit_vertex_buffer_layout> native_buffers;
  try {
    native_attributes.reserve(variant->pipeline.vertex_buffers.size());
    native_buffers.reserve(variant->pipeline.vertex_buffers.size());
    for (const auto& source_buffer : variant->pipeline.vertex_buffers) {
      auto& attributes = native_attributes.emplace_back();
      attributes.reserve(source_buffer.attributes.size());
      for (const auto& source_attribute : source_buffer.attributes) {
        attributes.push_back(
            {source_attribute.location, source_attribute.format, source_attribute.offset, 0});
      }
      native_buffers.push_back({source_buffer.stride, source_buffer.step_mode,
                                static_cast<std::uint32_t>(attributes.size()), 0,
                                attributes.data()});
    }
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_shader_destroy(renderer_, replacement.fragment_shader));
    static_cast<void>(granit_shader_destroy(renderer_, replacement.vertex_shader));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  desc.layout = pipeline_layout_;
  desc.vertex_shader = replacement.vertex_shader;
  desc.fragment_shader = replacement.fragment_shader;
  desc.color_format_count = 1;
  desc.color_formats = &request.color_format;
  desc.depth_stencil_format = request.depth_stencil_format;
  desc.sample_count = request.sample_count;
  desc.vertex_buffer_layout_count = static_cast<std::uint32_t>(native_buffers.size());
  desc.vertex_buffer_layouts = native_buffers.data();
  desc.primitive = variant->pipeline.primitive;
  desc.depth = &variant->pipeline.depth;
  desc.color_blend_count = 1;
  desc.color_blends = &variant->pipeline.color_blend;
  result = granit_graphics_pipeline_create(renderer_, &desc, &replacement.pipeline);
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(granit_shader_destroy(renderer_, replacement.fragment_shader));
    static_cast<void>(granit_shader_destroy(renderer_, replacement.vertex_shader));
    return result;
  }
  try {
    cache_.push_back(replacement);
  } catch (const std::bad_alloc&) {
    static_cast<void>(granit_graphics_pipeline_destroy(renderer_, replacement.pipeline));
    static_cast<void>(granit_shader_destroy(renderer_, replacement.fragment_shader));
    static_cast<void>(granit_shader_destroy(renderer_, replacement.vertex_shader));
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  pipeline = replacement.pipeline;
  return GRANIT_SUCCESS;
}

std::size_t material_template_gpu::cached_pipeline_count() const noexcept {
  std::lock_guard lock{mutex_};
  return cache_.size();
}

} // namespace granit::material
