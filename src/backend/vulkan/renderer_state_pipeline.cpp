// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderer_state.h"
#include "backend/vulkan/renderer_state_utils.h"

#include "backend/vulkan/resources.h"
#include "backend/vulkan/result.h"
#include "backend/vulkan/surface.h"
#include "core/texture_format.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <future>
#include <limits>
#include <new>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>

namespace granit::detail {

namespace {

class vulkan_pipeline_warmup_completion final : public backend_pipeline_warmup_completion {
public:
  explicit vulkan_pipeline_warmup_completion(std::future<granit_result>&& result)
      : result_(std::move(result)) {}

  granit_result poll() noexcept override {
    if (!result_.valid())
      return GRANIT_ERROR_INTERNAL;
    if (result_.wait_for(std::chrono::seconds{0}) != std::future_status::ready)
      return GRANIT_ERROR_NOT_READY;
    try {
      return result_.get();
    } catch (const std::bad_alloc&) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    } catch (...) {
      return GRANIT_ERROR_INTERNAL;
    }
  }

private:
  std::future<granit_result> result_;
};

} // namespace

std::unique_ptr<backend_pipeline_layout_resource>
vulkan_renderer_state::allocate_pipeline_layout_resource() {
  return std::make_unique<vulkan_pipeline_layout_resource>(shared_from_this());
}

std::unique_ptr<backend_graphics_pipeline_resource>
vulkan_renderer_state::allocate_graphics_pipeline_resource() {
  return std::make_unique<vulkan_graphics_pipeline_resource>(shared_from_this());
}

granit_result vulkan_renderer_state::create_graphics_pipeline(
    const backend_graphics_pipeline_create_info& info,
    backend_graphics_pipeline_resource& pipeline) noexcept {
  return create_native_graphics_pipeline(
      info.layout, info.vertex_shader, info.vertex_entry, info.fragment_shader, info.fragment_entry,
      info.vertex_buffers, info.primitive, info.depth, info.depth_bias, info.color_blends,
      info.color_formats, info.depth_stencil_format, info.sample_count, pipeline);
}

granit_result vulkan_renderer_state::warmup_graphics_pipeline_async(
    const backend_graphics_pipeline_create_info& info,
    std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept {
  try {
    auto owner = shared_from_this();
    auto future = std::async(std::launch::async, [owner = std::move(owner), info]() {
      auto pipeline = owner->allocate_graphics_pipeline_resource();
      if (!pipeline)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      return owner->create_graphics_pipeline(info, *pipeline);
    });
    completion = std::make_unique<vulkan_pipeline_warmup_completion>(std::move(future));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result vulkan_renderer_state::warmup_compute_pipeline_async(
    backend_pipeline_layout_resource& layout, backend_shader_resource& shader,
    const char* entry_point,
    std::unique_ptr<backend_pipeline_warmup_completion>& completion) noexcept {
  try {
    auto owner = shared_from_this();
    auto future =
        std::async(std::launch::async, [owner = std::move(owner), &layout, &shader, entry_point]() {
          auto pipeline = owner->allocate_compute_pipeline_resource();
          if (!pipeline)
            return GRANIT_ERROR_OUT_OF_MEMORY;
          return owner->create_compute_pipeline(layout, shader, entry_point, *pipeline);
        });
    completion = std::make_unique<vulkan_pipeline_warmup_completion>(std::move(future));
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

std::unique_ptr<backend_compute_pipeline_resource>
vulkan_renderer_state::allocate_compute_pipeline_resource() {
  return std::make_unique<vulkan_compute_pipeline_resource>(shared_from_this());
}

granit_result vulkan_renderer_state::create_native_pipeline_layout(
    std::span<backend_bind_group_layout_resource* const> bind_group_layouts,
    backend_pipeline_layout_resource& layout_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  try {
    std::vector<VkDescriptorSetLayout> native_layouts;
    native_layouts.reserve(bind_group_layouts.size());
    for (auto* bind_group_layout : bind_group_layouts) {
      if (bind_group_layout == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      native_layouts.push_back(
          static_cast<vulkan_bind_group_layout_resource&>(*bind_group_layout).native());
    }
    auto& layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    info.setLayoutCount = static_cast<std::uint32_t>(native_layouts.size());
    info.pSetLayouts = native_layouts.data();
    std::lock_guard lock{resource_mutex_};
    return observe_device_result(map_vulkan_result(device_.functions().vkCreatePipelineLayout(
        device_.native_handle(), &info, nullptr, &layout)));
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void vulkan_renderer_state::destroy_native_pipeline_layout(VkPipelineLayout layout) noexcept {
  if (layout != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipelineLayout(device_.native_handle(), layout, nullptr);
  }
}

granit_result vulkan_renderer_state::create_native_graphics_pipeline(
    backend_pipeline_layout_resource& layout_resource, backend_shader_resource& vertex_resource,
    const char* vertex_entry, backend_shader_resource& fragment_resource,
    const char* fragment_entry, std::span<const granit_vertex_buffer_layout> vertex_buffers,
    granit_primitive_state primitive, granit_depth_state depth_state,
    const granit_depth_bias_state* depth_bias,
    std::span<const granit_color_blend_state> color_blends,
    std::span<const granit_texture_format> color_formats,
    granit_texture_format depth_stencil_format, granit_sample_count sample_count,
    backend_graphics_pipeline_resource& pipeline_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
  const auto vertex_shader = static_cast<vulkan_shader_resource&>(vertex_resource).native();
  const auto fragment_shader = static_cast<vulkan_shader_resource&>(fragment_resource).native();
  auto& pipeline = static_cast<vulkan_graphics_pipeline_resource&>(pipeline_resource).native();
  std::array<VkPipelineShaderStageCreateInfo, 2> stages{};
  stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
  stages[0].module = vertex_shader;
  stages[0].pName = vertex_entry;
  stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  stages[1].module = fragment_shader;
  stages[1].pName = fragment_entry;

  const auto& limits = device_.properties().limits;
  std::size_t attribute_count{};
  for (const auto& buffer : vertex_buffers)
    attribute_count += buffer.attribute_count;
  if (vertex_buffers.size() > limits.maxVertexInputBindings ||
      attribute_count > limits.maxVertexInputAttributes)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  std::vector<VkVertexInputBindingDescription> bindings;
  std::vector<VkVertexInputAttributeDescription> attributes;
  bindings.reserve(vertex_buffers.size());
  attributes.reserve(attribute_count);
  for (std::uint32_t binding = 0; binding < vertex_buffers.size(); ++binding) {
    const auto& source = vertex_buffers[binding];
    if (source.stride > limits.maxVertexInputBindingStride)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    bindings.push_back({.binding = binding,
                        .stride = source.stride,
                        .inputRate = source.step_mode == GRANIT_VERTEX_STEP_MODE_INSTANCE
                                         ? VK_VERTEX_INPUT_RATE_INSTANCE
                                         : VK_VERTEX_INPUT_RATE_VERTEX});
    for (std::uint32_t index = 0; index < source.attribute_count; ++index) {
      const auto& attribute = source.attributes[index];
      if (attribute.offset > limits.maxVertexInputAttributeOffset)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      attributes.push_back({.location = attribute.location,
                            .binding = binding,
                            .format = map_vertex_format(attribute.format),
                            .offset = attribute.offset});
    }
  }
  VkPipelineVertexInputStateCreateInfo vertex_input{};
  vertex_input.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input.vertexBindingDescriptionCount = static_cast<std::uint32_t>(bindings.size());
  vertex_input.pVertexBindingDescriptions = bindings.data();
  vertex_input.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
  vertex_input.pVertexAttributeDescriptions = attributes.data();
  VkPipelineInputAssemblyStateCreateInfo input_assembly{};
  input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  constexpr std::array topologies{VK_PRIMITIVE_TOPOLOGY_POINT_LIST, VK_PRIMITIVE_TOPOLOGY_LINE_LIST,
                                  VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                  VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP};
  input_assembly.topology = topologies[primitive.topology - GRANIT_PRIMITIVE_TOPOLOGY_POINT_LIST];
  VkPipelineViewportStateCreateInfo viewport{};
  viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport.viewportCount = 1;
  viewport.scissorCount = 1;
  VkPipelineRasterizationStateCreateInfo rasterization{};
  rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  constexpr std::array polygon_modes{VK_POLYGON_MODE_FILL, VK_POLYGON_MODE_LINE,
                                     VK_POLYGON_MODE_POINT};
  constexpr std::array cull_modes{VK_CULL_MODE_NONE, VK_CULL_MODE_FRONT_BIT, VK_CULL_MODE_BACK_BIT,
                                  VK_CULL_MODE_FRONT_AND_BACK};
  constexpr std::array front_faces{VK_FRONT_FACE_COUNTER_CLOCKWISE, VK_FRONT_FACE_CLOCKWISE};
  if (primitive.polygon_mode != GRANIT_POLYGON_MODE_FILL &&
      !device_.fill_mode_non_solid_supported())
    return GRANIT_ERROR_UNSUPPORTED;
  rasterization.polygonMode = polygon_modes[primitive.polygon_mode - GRANIT_POLYGON_MODE_FILL];
  rasterization.cullMode = cull_modes[primitive.cull_mode - GRANIT_CULL_MODE_NONE];
  rasterization.frontFace = front_faces[primitive.front_face - GRANIT_FRONT_FACE_COUNTER_CLOCKWISE];
  rasterization.lineWidth = 1.0F;
  if (depth_bias) {
    rasterization.depthBiasEnable = VK_TRUE;
    rasterization.depthBiasConstantFactor = depth_bias->constant_factor;
    rasterization.depthBiasSlopeFactor = depth_bias->slope_factor;
    rasterization.depthBiasClamp = depth_bias->clamp;
  }
  VkPipelineMultisampleStateCreateInfo multisample{};
  multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample.rasterizationSamples = static_cast<VkSampleCountFlagBits>(sample_count);
  VkPipelineDepthStencilStateCreateInfo depth{};
  depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  constexpr std::array compare_operations{VK_COMPARE_OP_NEVER,
                                          VK_COMPARE_OP_LESS,
                                          VK_COMPARE_OP_EQUAL,
                                          VK_COMPARE_OP_LESS_OR_EQUAL,
                                          VK_COMPARE_OP_GREATER,
                                          VK_COMPARE_OP_NOT_EQUAL,
                                          VK_COMPARE_OP_GREATER_OR_EQUAL,
                                          VK_COMPARE_OP_ALWAYS};
  depth.depthTestEnable = depth_state.test_enabled != 0;
  depth.depthWriteEnable = depth_state.write_enabled != 0;
  depth.depthCompareOp = compare_operations[depth_state.compare - GRANIT_COMPARE_OPERATION_NEVER];
  std::vector<VkPipelineColorBlendAttachmentState> blend_attachments(color_formats.size());
  constexpr std::array blend_factors{
      VK_BLEND_FACTOR_ZERO,      VK_BLEND_FACTOR_ONE,
      VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
      VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
      VK_BLEND_FACTOR_DST_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA};
  constexpr std::array blend_operations{VK_BLEND_OP_ADD, VK_BLEND_OP_SUBTRACT,
                                        VK_BLEND_OP_REVERSE_SUBTRACT, VK_BLEND_OP_MIN,
                                        VK_BLEND_OP_MAX};
  for (std::size_t index = 0; index < blend_attachments.size(); ++index) {
    auto& target = blend_attachments[index];
    const auto& source = color_blends[index];
    target.blendEnable = source.enabled != 0;
    target.srcColorBlendFactor =
        blend_factors[source.source_color_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.dstColorBlendFactor =
        blend_factors[source.destination_color_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.colorBlendOp = blend_operations[source.color_operation - GRANIT_BLEND_OPERATION_ADD];
    target.srcAlphaBlendFactor =
        blend_factors[source.source_alpha_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.dstAlphaBlendFactor =
        blend_factors[source.destination_alpha_factor - GRANIT_BLEND_FACTOR_ZERO];
    target.alphaBlendOp = blend_operations[source.alpha_operation - GRANIT_BLEND_OPERATION_ADD];
    target.colorWriteMask = 0;
    if ((source.write_mask & GRANIT_COLOR_WRITE_RED_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_GREEN_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_BLUE_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
    if ((source.write_mask & GRANIT_COLOR_WRITE_ALPHA_BIT) != 0)
      target.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;
  }
  VkPipelineColorBlendStateCreateInfo blend{};
  blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  blend.attachmentCount = static_cast<std::uint32_t>(blend_attachments.size());
  blend.pAttachments = blend_attachments.data();
  const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic{};
  dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
  dynamic.pDynamicStates = dynamic_states.data();
  std::vector<VkFormat> native_formats;
  native_formats.reserve(color_formats.size());
  for (const auto format : color_formats)
    native_formats.push_back(map_texture_format(format));
  VkPipelineRenderingCreateInfo rendering{};
  rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  rendering.colorAttachmentCount = static_cast<std::uint32_t>(native_formats.size());
  rendering.pColorAttachmentFormats = native_formats.data();
  const auto depth_format = map_texture_format(depth_stencil_format);
  rendering.depthAttachmentFormat = depth_format;
  if (depth_stencil_format == GRANIT_TEXTURE_FORMAT_D24_UNORM_S8_UINT ||
      depth_stencil_format == GRANIT_TEXTURE_FORMAT_D32_FLOAT_S8_UINT)
    rendering.stencilAttachmentFormat = depth_format;

  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.pNext = &rendering;
  info.stageCount = static_cast<std::uint32_t>(stages.size());
  info.pStages = stages.data();
  info.pVertexInputState = &vertex_input;
  info.pInputAssemblyState = &input_assembly;
  info.pViewportState = &viewport;
  info.pRasterizationState = &rasterization;
  info.pMultisampleState = &multisample;
  info.pDepthStencilState = &depth;
  info.pColorBlendState = &blend;
  info.pDynamicState = &dynamic;
  info.layout = layout;
  std::lock_guard lock{pipeline_cache_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateGraphicsPipelines(
      device_.native_handle(), pipeline_cache_, 1, &info, nullptr, &pipeline)));
}

void vulkan_renderer_state::destroy_native_graphics_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

granit_result vulkan_renderer_state::create_native_compute_pipeline(
    backend_pipeline_layout_resource& layout_resource, backend_shader_resource& compute_resource,
    const char* compute_entry, backend_compute_pipeline_resource& pipeline_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto layout = static_cast<vulkan_pipeline_layout_resource&>(layout_resource).native();
  const auto compute_shader = static_cast<vulkan_shader_resource&>(compute_resource).native();
  auto& pipeline = static_cast<vulkan_compute_pipeline_resource&>(pipeline_resource).native();
  VkComputePipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  info.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  info.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  info.stage.module = compute_shader;
  info.stage.pName = compute_entry;
  info.layout = layout;
  std::lock_guard lock{pipeline_cache_mutex_};
  return observe_device_result(map_vulkan_result(device_.functions().vkCreateComputePipelines(
      device_.native_handle(), pipeline_cache_, 1, &info, nullptr, &pipeline)));
}

void vulkan_renderer_state::destroy_native_compute_pipeline(VkPipeline pipeline) noexcept {
  if (pipeline != VK_NULL_HANDLE) {
    std::lock_guard lock{resource_mutex_};
    device_.functions().vkDestroyPipeline(device_.native_handle(), pipeline, nullptr);
  }
}

} // namespace granit::detail
