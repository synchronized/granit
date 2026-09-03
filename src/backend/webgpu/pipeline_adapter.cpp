// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/webgpu/pipeline_adapter.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>
#include <vector>

namespace granit::detail {

struct webgpu_pipeline_context {
  backend_plugin_loader* loader{};
  granit_backend_plugin_instance instance{};
};

namespace {

class webgpu_pipeline_layout_resource final : public backend_pipeline_layout_resource {
public:
  explicit webgpu_pipeline_layout_resource(std::shared_ptr<webgpu_pipeline_context> context)
      : context_(std::move(context)) {}
  ~webgpu_pipeline_layout_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_pipeline_layout(context_->instance, handle_));
    }
  }
  std::shared_ptr<webgpu_pipeline_context> context_;
  granit_backend_plugin_pipeline_layout handle_{};
};

class webgpu_graphics_pipeline_resource final : public backend_graphics_pipeline_resource {
public:
  explicit webgpu_graphics_pipeline_resource(std::shared_ptr<webgpu_pipeline_context> context)
      : context_(std::move(context)) {}
  ~webgpu_graphics_pipeline_resource() override {
    if (handle_ != 0) {
      static_cast<void>(context_->loader->destroy_render_pipeline(context_->instance, handle_));
    }
  }
  std::shared_ptr<webgpu_pipeline_context> context_;
  granit_backend_plugin_render_pipeline handle_{};
};

class webgpu_compute_pipeline_resource final : public backend_compute_pipeline_resource {
public:
  explicit webgpu_compute_pipeline_resource(std::shared_ptr<webgpu_pipeline_context> context)
      : context_(std::move(context)) {}
  ~webgpu_compute_pipeline_resource() override {
    if (handle_ != 0)
      static_cast<void>(context_->loader->destroy_compute_pipeline(context_->instance, handle_));
  }
  std::shared_ptr<webgpu_pipeline_context> context_;
  granit_backend_plugin_compute_pipeline handle_{};
};

webgpu_pipeline_layout_resource* as_layout(backend_pipeline_layout_resource& resource) {
  return dynamic_cast<webgpu_pipeline_layout_resource*>(&resource);
}

webgpu_graphics_pipeline_resource* as_pipeline(backend_graphics_pipeline_resource& resource) {
  return dynamic_cast<webgpu_graphics_pipeline_resource*>(&resource);
}

std::uint32_t to_plugin_format(granit_texture_format format) noexcept {
  switch (format) {
  case GRANIT_TEXTURE_FORMAT_RGBA8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_BGRA8_UNORM:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_BGRA8_UNORM;
  case GRANIT_TEXTURE_FORMAT_D32_FLOAT:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_D32_FLOAT;
  case GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT:
    return GRANIT_BACKEND_PLUGIN_TEXTURE_FORMAT_RGBA16_FLOAT;
  default:
    return 0;
  }
}

granit_backend_plugin_blend_factor to_plugin_blend_factor(granit_blend_factor factor) noexcept {
  switch (factor) {
  case GRANIT_BLEND_FACTOR_ZERO:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ZERO;
  case GRANIT_BLEND_FACTOR_ONE:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE;
  case GRANIT_BLEND_FACTOR_SOURCE_COLOR:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_SOURCE_COLOR;
  case GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_SOURCE_COLOR;
  case GRANIT_BLEND_FACTOR_SOURCE_ALPHA:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_SOURCE_ALPHA;
  case GRANIT_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_SOURCE_ALPHA;
  case GRANIT_BLEND_FACTOR_DESTINATION_COLOR:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_DESTINATION_COLOR;
  case GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_DESTINATION_COLOR;
  case GRANIT_BLEND_FACTOR_DESTINATION_ALPHA:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_DESTINATION_ALPHA;
  case GRANIT_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA:
    return GRANIT_BACKEND_PLUGIN_BLEND_FACTOR_ONE_MINUS_DESTINATION_ALPHA;
  default:
    return 0;
  }
}

granit_backend_plugin_blend_operation
to_plugin_blend_operation(granit_blend_operation operation) noexcept {
  switch (operation) {
  case GRANIT_BLEND_OPERATION_ADD:
    return GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_ADD;
  case GRANIT_BLEND_OPERATION_SUBTRACT:
    return GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_SUBTRACT;
  case GRANIT_BLEND_OPERATION_REVERSE_SUBTRACT:
    return GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_REVERSE_SUBTRACT;
  case GRANIT_BLEND_OPERATION_MIN:
    return GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_MIN;
  case GRANIT_BLEND_OPERATION_MAX:
    return GRANIT_BACKEND_PLUGIN_BLEND_OPERATION_MAX;
  default:
    return 0;
  }
}

std::uint32_t to_plugin_color_write_mask(granit_color_write_mask mask) noexcept {
  std::uint32_t result{};
  if ((mask & GRANIT_COLOR_WRITE_RED_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_COLOR_WRITE_RED_BIT;
  if ((mask & GRANIT_COLOR_WRITE_GREEN_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_COLOR_WRITE_GREEN_BIT;
  if ((mask & GRANIT_COLOR_WRITE_BLUE_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_COLOR_WRITE_BLUE_BIT;
  if ((mask & GRANIT_COLOR_WRITE_ALPHA_BIT) != 0)
    result |= GRANIT_BACKEND_PLUGIN_COLOR_WRITE_ALPHA_BIT;
  return result;
}

} // namespace

webgpu_pipeline_adapter::webgpu_pipeline_adapter(backend_plugin_loader& loader,
                                                 granit_backend_plugin_instance instance)
    : context_(
          std::make_shared<webgpu_pipeline_context>(webgpu_pipeline_context{&loader, instance})) {}

std::unique_ptr<backend_pipeline_layout_resource>
webgpu_pipeline_adapter::allocate_pipeline_layout() const {
  return std::make_unique<webgpu_pipeline_layout_resource>(context_);
}

std::unique_ptr<backend_graphics_pipeline_resource>
webgpu_pipeline_adapter::allocate_graphics_pipeline() const {
  return std::make_unique<webgpu_graphics_pipeline_resource>(context_);
}

std::unique_ptr<backend_compute_pipeline_resource>
webgpu_pipeline_adapter::allocate_compute_pipeline() const {
  return std::make_unique<webgpu_compute_pipeline_resource>(context_);
}

granit_result webgpu_pipeline_adapter::validate_graphics_pipeline(
    const granit_graphics_pipeline_desc& desc) const noexcept {
  if (desc.color_format_count > 1 ||
      (desc.color_format_count == 0 &&
       desc.depth_stencil_format == GRANIT_TEXTURE_FORMAT_UNDEFINED) ||
      (desc.color_format_count == 1 && desc.color_formats[0] != GRANIT_TEXTURE_FORMAT_RGBA8_UNORM &&
       desc.color_formats[0] != GRANIT_TEXTURE_FORMAT_BGRA8_UNORM &&
       desc.color_formats[0] != GRANIT_TEXTURE_FORMAT_RGBA16_FLOAT) ||
      (desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_UNDEFINED &&
       desc.depth_stencil_format != GRANIT_TEXTURE_FORMAT_D32_FLOAT) ||
      (desc.sample_count != 1 && desc.sample_count != 4) ||
      desc.primitive.topology != GRANIT_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST ||
      desc.primitive.cull_mode == GRANIT_CULL_MODE_FRONT_AND_BACK ||
      desc.primitive.polygon_mode != GRANIT_POLYGON_MODE_FILL) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return GRANIT_SUCCESS;
}

granit_result webgpu_pipeline_adapter::create_pipeline_layout(
    std::span<const granit_backend_plugin_bind_group_layout> layouts,
    backend_pipeline_layout_resource& resource) const noexcept {
  auto* layout = as_layout(resource);
  if (layout == nullptr || layout->handle_ != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const granit_backend_plugin_pipeline_layout_desc desc{
      sizeof(granit_backend_plugin_pipeline_layout_desc),
      static_cast<std::uint32_t>(layouts.size()), layouts.data(), 0};
  return context_->loader->create_pipeline_layout(context_->instance, &desc, &layout->handle_);
}

granit_backend_plugin_pipeline_layout webgpu_pipeline_adapter::native_pipeline_layout(
    backend_pipeline_layout_resource& resource) const noexcept {
  const auto* layout = as_layout(resource);
  return layout == nullptr ? 0 : layout->handle_;
}

granit_result webgpu_pipeline_adapter::create_compute_pipeline(
    backend_compute_pipeline_resource& resource, granit_backend_plugin_pipeline_layout layout,
    granit_backend_plugin_shader shader) const noexcept {
  auto* pipeline = dynamic_cast<webgpu_compute_pipeline_resource*>(&resource);
  if (pipeline == nullptr || pipeline->handle_ != 0 || layout == 0 || shader == 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const granit_backend_plugin_compute_pipeline_desc desc{sizeof(desc), 0, layout, shader};
  return context_->loader->create_compute_pipeline(context_->instance, &desc, &pipeline->handle_);
}

granit_backend_plugin_compute_pipeline webgpu_pipeline_adapter::native_compute_pipeline(
    backend_compute_pipeline_resource& resource) const noexcept {
  const auto* pipeline = dynamic_cast<webgpu_compute_pipeline_resource*>(&resource);
  return pipeline == nullptr ? 0 : pipeline->handle_;
}

granit_result webgpu_pipeline_adapter::create_graphics_pipeline(
    backend_graphics_pipeline_resource& resource, backend_pipeline_layout_resource& layout_resource,
    granit_backend_plugin_shader vertex_shader, granit_backend_plugin_shader fragment_shader,
    std::span<const granit_vertex_buffer_layout> vertex_buffers, granit_texture_format color_format,
    granit_texture_format depth_stencil_format, granit_sample_count sample_count,
    const granit_primitive_state& primitive, const granit_depth_state& depth,
    const granit_depth_bias_state* depth_bias,
    const granit_color_blend_state& color_blend) const noexcept {
  auto* pipeline = as_pipeline(resource);
  auto* layout = as_layout(layout_resource);
  const auto plugin_format = to_plugin_format(color_format);
  if (pipeline == nullptr || pipeline->handle_ != 0 || layout == nullptr || layout->handle_ == 0 ||
      vertex_shader == 0 || fragment_shader == 0 ||
      (color_format != GRANIT_TEXTURE_FORMAT_UNDEFINED && plugin_format == 0)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    std::vector<granit_backend_plugin_vertex_buffer_layout> layouts;
    std::vector<granit_backend_plugin_vertex_attribute> attributes;
    layouts.reserve(vertex_buffers.size());
    std::size_t attribute_count = 0;
    for (const auto& source : vertex_buffers) {
      if (source.attribute_count > std::numeric_limits<std::size_t>::max() - attribute_count)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      attribute_count += source.attribute_count;
    }
    // 布局保存 attributes 中元素的地址，因此必须在写入任何布局前一次性完成容量分配。
    attributes.reserve(attribute_count);
    for (const auto& source : vertex_buffers) {
      const auto first = attributes.size();
      for (std::uint32_t index = 0; index < source.attribute_count; ++index) {
        const auto& attribute = source.attributes[index];
        attributes.push_back(
            {attribute.location, attribute.format, attribute.offset, attribute.reserved});
      }
      layouts.push_back({source.stride, source.step_mode, source.attribute_count, source.reserved,
                         attributes.data() + first});
    }
    const auto rounded_bias =
        depth_bias == nullptr ? 0.0 : std::round(static_cast<double>(depth_bias->constant_factor));
    const auto constant_bias = static_cast<std::int32_t>(
        std::clamp(rounded_bias, static_cast<double>(std::numeric_limits<std::int32_t>::min()),
                   static_cast<double>(std::numeric_limits<std::int32_t>::max())));
    const granit_backend_plugin_render_pipeline_desc desc{
        sizeof(desc),
        0,
        layout->handle_,
        vertex_shader,
        fragment_shader,
        plugin_format,
        static_cast<std::uint32_t>(layouts.size()),
        layouts.data(),
        to_plugin_format(depth_stencil_format),
        depth.test_enabled,
        depth.write_enabled,
        depth.compare,
        constant_bias,
        depth_bias == nullptr ? 0.0F : depth_bias->slope_factor,
        depth_bias == nullptr ? 0.0F : depth_bias->clamp,
        color_blend.enabled,
        to_plugin_blend_factor(color_blend.source_color_factor),
        to_plugin_blend_factor(color_blend.destination_color_factor),
        to_plugin_blend_operation(color_blend.color_operation),
        to_plugin_blend_factor(color_blend.source_alpha_factor),
        to_plugin_blend_factor(color_blend.destination_alpha_factor),
        to_plugin_blend_operation(color_blend.alpha_operation),
        to_plugin_color_write_mask(color_blend.write_mask),
        GRANIT_BACKEND_PLUGIN_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        primitive.front_face == GRANIT_FRONT_FACE_COUNTER_CLOCKWISE
            ? GRANIT_BACKEND_PLUGIN_FRONT_FACE_COUNTER_CLOCKWISE
            : GRANIT_BACKEND_PLUGIN_FRONT_FACE_CLOCKWISE,
        primitive.cull_mode == GRANIT_CULL_MODE_NONE
            ? GRANIT_BACKEND_PLUGIN_CULL_MODE_NONE
            : (primitive.cull_mode == GRANIT_CULL_MODE_FRONT
                   ? GRANIT_BACKEND_PLUGIN_CULL_MODE_FRONT
                   : GRANIT_BACKEND_PLUGIN_CULL_MODE_BACK),
        GRANIT_BACKEND_PLUGIN_POLYGON_MODE_FILL,
        sample_count};
    return context_->loader->create_render_pipeline(context_->instance, &desc, &pipeline->handle_);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_backend_plugin_render_pipeline webgpu_pipeline_adapter::native_handle(
    backend_graphics_pipeline_resource& resource) const noexcept {
  const auto* pipeline = as_pipeline(resource);
  return pipeline == nullptr ? 0 : pipeline->handle_;
}

} // namespace granit::detail
