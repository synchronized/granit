// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/bindless_resource_registry.h"
#include "backend/vulkan/command_recorder.h"
#include "backend/vulkan/device.h"
#include "backend/vulkan/instance.h"
#include "backend/vulkan/loader.h"
#include "backend/vulkan/memory_allocator.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <vector>

#include <catch2/catch_all.hpp>

namespace {

using granit::detail::initialize_vulkan_loader;
using granit::detail::vulkan_bindless_resource_registry;
using granit::detail::vulkan_buffer_allocation;
using granit::detail::vulkan_command_recorder;
using granit::detail::vulkan_device;
using granit::detail::vulkan_image_allocation;
using granit::detail::vulkan_instance;
using granit::detail::vulkan_memory_allocator;
using granit::detail::vulkan_memory_location;

constexpr std::uint32_t image_width = 1;
constexpr std::uint32_t image_height = 1;

#if defined(GRANIT_BINDLESS_SPIRV) && defined(GRANIT_BINDGROUP_SPIRV)
std::vector<std::byte> read_spirv(const char* path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    result[index] = static_cast<std::byte>(bytes[index]);
  return result;
}
#endif

TEST_CASE("Vulkan Bindless Shader 按索引采样 texture 和 sampler", "[vulkan][bindless][gpu]") {
#if !defined(GRANIT_BINDLESS_SPIRV) || !defined(GRANIT_BINDGROUP_SPIRV)
  SKIP("当前构建没有 glslc，未生成 Vulkan Bindless 实验 Shader");
#else
  const auto loader = initialize_vulkan_loader();
  if (loader.result != GRANIT_SUCCESS)
    SKIP("当前运行环境没有可用的 Vulkan 1.3 loader");

  vulkan_instance instance;
  REQUIRE(instance.initialize({.application_name = "granit-bindless-shader-tests"}) ==
          GRANIT_SUCCESS);
  vulkan_device device;
  const auto device_result = device.initialize(instance);
  if (device_result == GRANIT_ERROR_NO_SUITABLE_DEVICE)
    SKIP("当前运行环境没有满足 Granit Vulkan 1.3 要求的图形设备");
  REQUIRE(device_result == GRANIT_SUCCESS);
  if (!device.bindless_descriptor_indexing_supported())
    SKIP("当前 Vulkan 设备不支持 Granit Bindless 前置特性");

  vulkan_memory_allocator allocator;
  REQUIRE(allocator.initialize(instance, device) == GRANIT_SUCCESS);
  vulkan_bindless_resource_registry registry{UINT64_C(0x534F)};
  REQUIRE(registry.initialize(device, 2, 2) == GRANIT_SUCCESS);

  const auto create_image = [&](VkImageUsageFlags usage, vulkan_image_allocation& image) {
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.extent = {image_width, image_height, 1};
    info.mipLevels = 1;
    info.arrayLayers = 1;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.tiling = VK_IMAGE_TILING_OPTIMAL;
    info.usage = usage;
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return allocator.create_image(info, vulkan_memory_location::device, image);
  };
  const auto create_view = [&](VkImage image, VkImageView& view) {
    VkImageViewCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    info.image = image;
    info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    info.format = VK_FORMAT_R8G8B8A8_UNORM;
    info.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    return device.functions().vkCreateImageView(device.native_handle(), &info, nullptr, &view);
  };

  vulkan_image_allocation source_image;
  vulkan_image_allocation output_image;
  REQUIRE(create_image(VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                       source_image) == GRANIT_SUCCESS);
  REQUIRE(create_image(VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                       output_image) == GRANIT_SUCCESS);
  VkImageView source_view = VK_NULL_HANDLE;
  VkImageView output_view = VK_NULL_HANDLE;
  REQUIRE(create_view(source_image.image, source_view) == VK_SUCCESS);
  REQUIRE(create_view(output_image.image, output_view) == VK_SUCCESS);

  VkSamplerCreateInfo sampler_info{};
  sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_info.magFilter = VK_FILTER_NEAREST;
  sampler_info.minFilter = VK_FILTER_NEAREST;
  sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
  sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_info.maxLod = 1.0F;
  VkSampler sampler = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateSampler(device.native_handle(), &sampler_info, nullptr,
                                             &sampler) == VK_SUCCESS);
  std::uint64_t texture_handle{};
  std::uint64_t sampler_handle{};
  REQUIRE(registry.register_sampled_texture(device, source_view, 1, texture_handle) ==
          GRANIT_SUCCESS);
  REQUIRE(registry.register_sampler(device, sampler, 1, sampler_handle) == GRANIT_SUCCESS);
  CHECK(static_cast<std::uint32_t>(texture_handle) == 1);
  CHECK(static_cast<std::uint32_t>(sampler_handle) == 1);

  const std::array<VkDescriptorSetLayoutBinding, 2> traditional_bindings{
      VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                                   VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
      VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT,
                                   nullptr}};
  VkDescriptorSetLayout traditional_layout = VK_NULL_HANDLE;
  VkDescriptorSetLayoutCreateInfo traditional_layout_info{};
  traditional_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  traditional_layout_info.bindingCount = static_cast<std::uint32_t>(traditional_bindings.size());
  traditional_layout_info.pBindings = traditional_bindings.data();
  REQUIRE(device.functions().vkCreateDescriptorSetLayout(device.native_handle(),
                                                         &traditional_layout_info, nullptr,
                                                         &traditional_layout) == VK_SUCCESS);
  const std::array<VkDescriptorPoolSize, 2> traditional_pool_sizes{
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1},
      VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 1}};
  VkDescriptorPoolCreateInfo traditional_pool_info{};
  traditional_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  traditional_pool_info.maxSets = 1;
  traditional_pool_info.poolSizeCount = static_cast<std::uint32_t>(traditional_pool_sizes.size());
  traditional_pool_info.pPoolSizes = traditional_pool_sizes.data();
  VkDescriptorPool traditional_pool = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateDescriptorPool(device.native_handle(), &traditional_pool_info,
                                                    nullptr, &traditional_pool) == VK_SUCCESS);
  VkDescriptorSetAllocateInfo traditional_allocate{};
  traditional_allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  traditional_allocate.descriptorPool = traditional_pool;
  traditional_allocate.descriptorSetCount = 1;
  traditional_allocate.pSetLayouts = &traditional_layout;
  VkDescriptorSet traditional_set = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkAllocateDescriptorSets(device.native_handle(), &traditional_allocate,
                                                      &traditional_set) == VK_SUCCESS);
  const VkDescriptorImageInfo sampled_image{VK_NULL_HANDLE, source_view,
                                            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
  const VkDescriptorImageInfo sampled_sampler{sampler, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
  std::array<VkWriteDescriptorSet, 2> traditional_writes{};
  traditional_writes[0] = {
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,        traditional_set, 0,      0, 1,
      VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,       &sampled_image, nullptr,         nullptr};
  traditional_writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                           nullptr,
                           traditional_set,
                           1,
                           0,
                           1,
                           VK_DESCRIPTOR_TYPE_SAMPLER,
                           &sampled_sampler,
                           nullptr,
                           nullptr};
  device.functions().vkUpdateDescriptorSets(device.native_handle(),
                                            static_cast<std::uint32_t>(traditional_writes.size()),
                                            traditional_writes.data(), 0, nullptr);
  std::array<VkWriteDescriptorSet, 2> bindless_writes = traditional_writes;
  bindless_writes[0].dstSet = registry.descriptor_set();
  bindless_writes[0].dstBinding = 0;
  bindless_writes[0].dstArrayElement = 1;
  bindless_writes[1].dstSet = registry.descriptor_set();
  bindless_writes[1].dstBinding = 1;
  bindless_writes[1].dstArrayElement = 1;
  constexpr std::uint32_t update_iterations = 256;
  const auto measure_updates = [&](std::span<const VkWriteDescriptorSet> writes) {
    const auto start = std::chrono::steady_clock::now();
    for (std::uint32_t iteration = 0; iteration < update_iterations; ++iteration) {
      device.functions().vkUpdateDescriptorSets(device.native_handle(),
                                                static_cast<std::uint32_t>(writes.size()),
                                                writes.data(), 0, nullptr);
    }
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() -
                                                                start)
        .count();
  };
  const auto bindless_update_ns = measure_updates(bindless_writes);
  const auto traditional_update_ns = measure_updates(traditional_writes);
  std::cout << "bindless_update_ns=" << bindless_update_ns
            << ",traditional_bind_group_update_ns=" << traditional_update_ns
            << ",iterations=" << update_iterations << '\n';

  VkBufferCreateInfo readback_info{};
  readback_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  readback_info.size = 4;
  readback_info.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
  vulkan_buffer_allocation readback;
  REQUIRE(allocator.create_buffer(readback_info, vulkan_memory_location::readback, readback) ==
          GRANIT_SUCCESS);

  const auto spirv = read_spirv(GRANIT_BINDLESS_SPIRV);
  REQUIRE(!spirv.empty());
  VkShaderModuleCreateInfo shader_info{};
  shader_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_info.codeSize = spirv.size();
  shader_info.pCode = reinterpret_cast<const std::uint32_t*>(spirv.data());
  VkShaderModule shader = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateShaderModule(device.native_handle(), &shader_info, nullptr,
                                                  &shader) == VK_SUCCESS);

  VkDescriptorSetLayoutBinding output_binding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                              VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
  VkDescriptorSetLayoutCreateInfo output_layout_info{};
  output_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  output_layout_info.bindingCount = 1;
  output_layout_info.pBindings = &output_binding;
  VkDescriptorSetLayout output_layout = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateDescriptorSetLayout(
              device.native_handle(), &output_layout_info, nullptr, &output_layout) == VK_SUCCESS);
  VkDescriptorPoolSize output_pool_size{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1};
  VkDescriptorPoolCreateInfo output_pool_info{};
  output_pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  output_pool_info.maxSets = 1;
  output_pool_info.poolSizeCount = 1;
  output_pool_info.pPoolSizes = &output_pool_size;
  VkDescriptorPool output_pool = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateDescriptorPool(device.native_handle(), &output_pool_info,
                                                    nullptr, &output_pool) == VK_SUCCESS);
  VkDescriptorSetAllocateInfo output_allocate{};
  output_allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  output_allocate.descriptorPool = output_pool;
  output_allocate.descriptorSetCount = 1;
  output_allocate.pSetLayouts = &output_layout;
  VkDescriptorSet output_set = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkAllocateDescriptorSets(device.native_handle(), &output_allocate,
                                                      &output_set) == VK_SUCCESS);
  VkDescriptorImageInfo output_info{VK_NULL_HANDLE, output_view, VK_IMAGE_LAYOUT_GENERAL};
  VkWriteDescriptorSet output_write{};
  output_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  output_write.dstSet = output_set;
  output_write.dstBinding = 0;
  output_write.descriptorCount = 1;
  output_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  output_write.pImageInfo = &output_info;
  device.functions().vkUpdateDescriptorSets(device.native_handle(), 1, &output_write, 0, nullptr);

  const std::array<VkDescriptorSetLayout, 2> layouts{registry.layout(), output_layout};
  VkPipelineLayoutCreateInfo pipeline_layout_info{};
  pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_info.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
  pipeline_layout_info.pSetLayouts = layouts.data();
  VkPipelineLayout pipeline_layout = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreatePipelineLayout(device.native_handle(), &pipeline_layout_info,
                                                    nullptr, &pipeline_layout) == VK_SUCCESS);
  VkPipelineShaderStageCreateInfo stage{};
  stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
  stage.module = shader;
  stage.pName = "main";
  VkComputePipelineCreateInfo pipeline_info{};
  pipeline_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
  pipeline_info.stage = stage;
  pipeline_info.layout = pipeline_layout;
  VkPipeline pipeline = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateComputePipelines(device.native_handle(), VK_NULL_HANDLE, 1,
                                                      &pipeline_info, nullptr,
                                                      &pipeline) == VK_SUCCESS);

  const auto traditional_spirv = read_spirv(GRANIT_BINDGROUP_SPIRV);
  REQUIRE(!traditional_spirv.empty());
  shader_info.codeSize = traditional_spirv.size();
  shader_info.pCode = reinterpret_cast<const std::uint32_t*>(traditional_spirv.data());
  VkShaderModule traditional_shader = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateShaderModule(device.native_handle(), &shader_info, nullptr,
                                                  &traditional_shader) == VK_SUCCESS);
  const std::array<VkDescriptorSetLayout, 2> traditional_layouts{traditional_layout, output_layout};
  pipeline_layout_info.setLayoutCount = static_cast<std::uint32_t>(traditional_layouts.size());
  pipeline_layout_info.pSetLayouts = traditional_layouts.data();
  VkPipelineLayout traditional_pipeline_layout = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreatePipelineLayout(device.native_handle(), &pipeline_layout_info,
                                                    nullptr,
                                                    &traditional_pipeline_layout) == VK_SUCCESS);
  stage.module = traditional_shader;
  pipeline_info.layout = traditional_pipeline_layout;
  VkPipeline traditional_pipeline = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateComputePipelines(device.native_handle(), VK_NULL_HANDLE, 1,
                                                      &pipeline_info, nullptr,
                                                      &traditional_pipeline) == VK_SUCCESS);

  vulkan_command_recorder recorder;
  REQUIRE(recorder.initialize(device) == GRANIT_SUCCESS);
  REQUIRE(recorder.begin(device) == GRANIT_SUCCESS);
  const VkClearColorValue clear_color{{0.0F, 1.0F, 0.0F, 1.0F}};
  const std::array<VkImageMemoryBarrier2, 2> begin_barriers{
      VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                            nullptr,
                            VK_PIPELINE_STAGE_2_NONE,
                            0,
                            VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                            VK_ACCESS_2_TRANSFER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                            VK_QUEUE_FAMILY_IGNORED,
                            VK_QUEUE_FAMILY_IGNORED,
                            source_image.image,
                            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}},
      VkImageMemoryBarrier2{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                            nullptr,
                            VK_PIPELINE_STAGE_2_NONE,
                            0,
                            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                            VK_ACCESS_2_SHADER_WRITE_BIT,
                            VK_IMAGE_LAYOUT_UNDEFINED,
                            VK_IMAGE_LAYOUT_GENERAL,
                            VK_QUEUE_FAMILY_IGNORED,
                            VK_QUEUE_FAMILY_IGNORED,
                            output_image.image,
                            {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}}};
  const VkImageSubresourceRange clear_range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  VkDependencyInfo begin_dependency{};
  begin_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  begin_dependency.imageMemoryBarrierCount = static_cast<std::uint32_t>(begin_barriers.size());
  begin_dependency.pImageMemoryBarriers = begin_barriers.data();
  device.functions().vkCmdPipelineBarrier2(recorder.native_handle(), &begin_dependency);
  device.functions().vkCmdClearColorImage(recorder.native_handle(), source_image.image,
                                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clear_color, 1,
                                          &clear_range);
  const VkImageMemoryBarrier2 source_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                             nullptr,
                                             VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                             VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             source_image.image,
                                             {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  VkDependencyInfo source_dependency{};
  source_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  source_dependency.imageMemoryBarrierCount = 1;
  source_dependency.pImageMemoryBarriers = &source_barrier;
  device.functions().vkCmdPipelineBarrier2(recorder.native_handle(), &source_dependency);
  device.functions().vkCmdBindPipeline(recorder.native_handle(), VK_PIPELINE_BIND_POINT_COMPUTE,
                                       pipeline);
  const std::array<VkDescriptorSet, 2> descriptor_sets{registry.descriptor_set(), output_set};
  device.functions().vkCmdBindDescriptorSets(
      recorder.native_handle(), VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0,
      static_cast<std::uint32_t>(descriptor_sets.size()), descriptor_sets.data(), 0, nullptr);
  device.functions().vkCmdDispatch(recorder.native_handle(), 1, 1, 1);
  const VkImageMemoryBarrier2 output_barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                                             nullptr,
                                             VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                                             VK_ACCESS_2_SHADER_WRITE_BIT,
                                             VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                             VK_ACCESS_2_TRANSFER_READ_BIT,
                                             VK_IMAGE_LAYOUT_GENERAL,
                                             VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             VK_QUEUE_FAMILY_IGNORED,
                                             output_image.image,
                                             {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
  VkDependencyInfo output_dependency{};
  output_dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  output_dependency.imageMemoryBarrierCount = 1;
  output_dependency.pImageMemoryBarriers = &output_barrier;
  device.functions().vkCmdPipelineBarrier2(recorder.native_handle(), &output_dependency);
  const VkBufferImageCopy readback_region{
      0, 4, 1, {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}, {0, 0, 0}, {image_width, image_height, 1}};
  device.functions().vkCmdCopyImageToBuffer(recorder.native_handle(), output_image.image,
                                            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readback.buffer,
                                            1, &readback_region);
  REQUIRE(recorder.end(device) == GRANIT_SUCCESS);

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  VkFence fence = VK_NULL_HANDLE;
  REQUIRE(device.functions().vkCreateFence(device.native_handle(), &fence_info, nullptr, &fence) ==
          VK_SUCCESS);
  VkCommandBufferSubmitInfo command_info{VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, nullptr,
                                         recorder.native_handle(), 0};
  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.commandBufferInfoCount = 1;
  submit_info.pCommandBufferInfos = &command_info;
  REQUIRE(device.functions().vkQueueSubmit2(device.graphics_queue(), 1, &submit_info, fence) ==
          VK_SUCCESS);
  REQUIRE(registry.release(texture_handle, granit::internal::resource_table_type::texture_view,
                           1) == GRANIT_SUCCESS);
  REQUIRE(registry.release(sampler_handle, granit::internal::resource_table_type::sampler, 1) ==
          GRANIT_SUCCESS);
  registry.collect(0);
  std::uint64_t pending_texture_handle{};
  REQUIRE(registry.register_sampled_texture(device, source_view, 2, pending_texture_handle) ==
          GRANIT_SUCCESS);
  CHECK(static_cast<std::uint32_t>(pending_texture_handle) == 2);
  REQUIRE(registry.release(pending_texture_handle,
                           granit::internal::resource_table_type::texture_view,
                           1) == GRANIT_SUCCESS);
  REQUIRE(device.functions().vkWaitForFences(device.native_handle(), 1, &fence, VK_TRUE,
                                             UINT64_MAX) == VK_SUCCESS);
  registry.collect(1);
  std::uint64_t recycled_texture_handle{};
  REQUIRE(registry.register_sampled_texture(device, source_view, 3, recycled_texture_handle) ==
          GRANIT_SUCCESS);
  CHECK(static_cast<std::uint32_t>(recycled_texture_handle) == 1);
  device.functions().vkDestroyFence(device.native_handle(), fence, nullptr);
  recorder.destroy(device);
  REQUIRE(allocator.invalidate(readback, 0, 4) == GRANIT_SUCCESS);
  const auto* pixels = static_cast<const std::uint8_t*>(readback.mapped_data);
  CHECK(pixels[0] == 0);
  CHECK(pixels[1] == 255);
  CHECK(pixels[2] == 0);
  CHECK(pixels[3] == 255);

  device.functions().vkDestroyPipeline(device.native_handle(), pipeline, nullptr);
  device.functions().vkDestroyPipelineLayout(device.native_handle(), pipeline_layout, nullptr);
  device.functions().vkDestroyShaderModule(device.native_handle(), shader, nullptr);
  device.functions().vkDestroyPipeline(device.native_handle(), traditional_pipeline, nullptr);
  device.functions().vkDestroyPipelineLayout(device.native_handle(), traditional_pipeline_layout,
                                             nullptr);
  device.functions().vkDestroyShaderModule(device.native_handle(), traditional_shader, nullptr);
  device.functions().vkDestroyDescriptorPool(device.native_handle(), output_pool, nullptr);
  device.functions().vkDestroyDescriptorSetLayout(device.native_handle(), output_layout, nullptr);
  device.functions().vkDestroyDescriptorPool(device.native_handle(), traditional_pool, nullptr);
  device.functions().vkDestroyDescriptorSetLayout(device.native_handle(), traditional_layout,
                                                  nullptr);
  registry.destroy(device);
  device.functions().vkDestroySampler(device.native_handle(), sampler, nullptr);
  device.functions().vkDestroyImageView(device.native_handle(), source_view, nullptr);
  device.functions().vkDestroyImageView(device.native_handle(), output_view, nullptr);
  allocator.destroy_image(source_image);
  allocator.destroy_image(output_image);
  allocator.destroy_buffer(readback);
  allocator.reset();
  SUCCEED("Vulkan descriptor array shader path completed without validation failure");
#endif
}

} // namespace
