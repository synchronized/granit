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

class callback_upload_completion final : public backend_upload_completion {
public:
  explicit callback_upload_completion(std::function<granit_result()> poll)
      : poll_(std::move(poll)) {}
  [[nodiscard]] granit_result poll() noexcept override { return poll_(); }

private:
  std::function<granit_result()> poll_;
};

class vulkan_readback_completion final : public backend_readback_completion {
public:
  using poll_function = std::function<granit_result()>;
  using copy_function = std::function<granit_result(std::uint32_t, void*, std::uint64_t)>;
  using info_function = std::function<granit_result(std::uint32_t, granit_readback_result_info&)>;
  using release_function = std::function<void()>;

  vulkan_readback_completion(poll_function poll, info_function info, copy_function copy,
                             release_function release)
      : poll_(std::move(poll)), info_(std::move(info)), copy_(std::move(copy)),
        release_(std::move(release)) {}
  ~vulkan_readback_completion() override { release_(); }
  [[nodiscard]] granit_result poll() noexcept override { return poll_(); }
  [[nodiscard]] granit_result
  get_result_info(std::uint32_t index, granit_readback_result_info& info) const noexcept override {
    return info_(index, info);
  }
  [[nodiscard]] granit_result copy_result(std::uint32_t index, void* data,
                                          std::uint64_t size) noexcept override {
    return copy_(index, data, size);
  }

private:
  poll_function poll_;
  info_function info_;
  copy_function copy_;
  release_function release_;
};

} // namespace

std::size_t vulkan_renderer_state::acquire_upload_slot(bool wait) {
  std::unique_lock lock{upload_mutex_};
  for (;;) {
    for (auto& slot : upload_slots_) {
      if (!slot.acquired || !slot.submitted)
        continue;
      const auto result = slot.context->wait(device_, 0);
      if (result == GRANIT_SUCCESS) {
        slot.acquired = false;
        slot.submitted = false;
      } else if (result != GRANIT_ERROR_NOT_READY) {
        static_cast<void>(observe_device_result(result));
      }
    }
    if (std::ranges::any_of(upload_slots_, [](const auto& slot) { return !slot.acquired; }))
      break;
    if (!wait)
      return SIZE_MAX;
    upload_available_.wait_for(lock, std::chrono::milliseconds(1));
  }
  const auto found =
      std::ranges::find_if(upload_slots_, [](const auto& slot) { return !slot.acquired; });
  found->acquired = true;
  found->submitted = false;
  ++found->generation;
  return static_cast<std::size_t>(std::distance(upload_slots_.begin(), found));
}

void vulkan_renderer_state::release_upload_slot(std::size_t index) noexcept {
  {
    std::lock_guard lock{upload_mutex_};
    upload_slots_[index].acquired = false;
    upload_slots_[index].submitted = false;
  }
  upload_available_.notify_one();
}

void vulkan_renderer_state::mark_upload_slot_submitted(std::size_t index) noexcept {
  std::lock_guard lock{upload_mutex_};
  upload_slots_[index].submitted = true;
}

granit_result vulkan_renderer_state::poll_upload_slot(std::size_t index,
                                                      std::uint64_t generation) noexcept {
  std::lock_guard lock{upload_mutex_};
  if (index >= upload_slots_.size())
    return GRANIT_ERROR_INVALID_HANDLE;
  auto& slot = upload_slots_[index];
  if (slot.generation != generation || !slot.acquired)
    return GRANIT_SUCCESS;
  if (!slot.submitted)
    return GRANIT_ERROR_NOT_READY;
  const auto result = slot.context->wait(device_, 0);
  if (result == GRANIT_SUCCESS) {
    slot.acquired = false;
    slot.submitted = false;
    upload_available_.notify_one();
  }
  return observe_device_result(result);
}

std::size_t vulkan_renderer_state::try_acquire_readback_slot() {
  std::lock_guard lock{readback_mutex_};
  const auto found =
      std::ranges::find_if(readback_slots_, [](const auto& slot) { return !slot.acquired; });
  if (found == readback_slots_.end())
    return SIZE_MAX;
  found->acquired = true;
  found->submitted = false;
  ++found->generation;
  return static_cast<std::size_t>(std::distance(readback_slots_.begin(), found));
}

void vulkan_renderer_state::release_readback_slot(std::size_t index,
                                                  std::uint64_t generation) noexcept {
  {
    std::lock_guard lock{readback_mutex_};
    if (index >= readback_slots_.size() || readback_slots_[index].generation != generation)
      return;
    readback_slots_[index].acquired = false;
    readback_slots_[index].submitted = false;
  }
  readback_available_.notify_one();
}

void vulkan_renderer_state::mark_readback_slot_submitted(std::size_t index) noexcept {
  std::lock_guard lock{readback_mutex_};
  readback_slots_[index].submitted = true;
}

granit_result vulkan_renderer_state::poll_readback_slot(std::size_t index,
                                                        std::uint64_t generation) noexcept {
  std::lock_guard lock{readback_mutex_};
  if (index >= readback_slots_.size())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& slot = readback_slots_[index];
  if (slot.generation != generation || !slot.acquired)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!slot.submitted)
    return GRANIT_ERROR_NOT_READY;
  return observe_device_result(slot.context->wait(device_, 0));
}

granit_result vulkan_renderer_state::upload_buffer(backend_buffer_resource& buffer_resource,
                                                   std::uint64_t offset, const void* data,
                                                   std::uint64_t size) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto& buffer = static_cast<vulkan_buffer_resource&>(buffer_resource).native();
  const auto slot_index = acquire_upload_slot(true);
  auto& context = *upload_slots_[slot_index].context;
  auto finish = [&](granit_result result) {
    release_upload_slot(slot_index);
    return observe_device_result(result);
  };
  auto result = context.ensure_capacity(memory_allocator_, size);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  std::memcpy(context.staging().mapped_data, data, static_cast<std::size_t>(size));
  result = memory_allocator_.flush(context.staging(), 0, size);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.begin(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  const auto& functions = device_.functions();
  const VkBufferCopy copy{.srcOffset = 0, .dstOffset = offset, .size = size};
  functions.vkCmdCopyBuffer(context.command_buffer(), context.staging().buffer, buffer.buffer, 1,
                            &copy);
  result = context.end(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  {
    std::lock_guard queue_lock{queue_mutex_};
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = context.command_buffer();
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    result = map_vulkan_result(
        functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, context.fence()));
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  return finish(context.wait(device_));
}

granit_result vulkan_renderer_state::upload_batch_async(
    std::span<const backend_upload_operation> uploads,
    std::unique_ptr<backend_upload_completion>& completion) noexcept {
  completion.reset();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (uploads.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  VkDeviceSize required{};
  const auto alignment =
      std::max<VkDeviceSize>(4, device_.properties().limits.optimalBufferCopyOffsetAlignment);
  for (const auto& upload : uploads) {
    if (upload.data == nullptr || upload.size == 0 ||
        (upload.type != backend_upload_type::buffer &&
         upload.type != backend_upload_type::texture) ||
        (upload.type == backend_upload_type::buffer && upload.buffer == nullptr) ||
        (upload.type == backend_upload_type::texture && upload.texture == nullptr)) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto aligned = (required + alignment - 1) & ~(alignment - 1);
    if (aligned < required || upload.size > UINT64_MAX - aligned)
      return GRANIT_ERROR_OUT_OF_MEMORY;
    required = aligned + upload.size;
  }

  const auto slot_index = acquire_upload_slot(false);
  if (slot_index == SIZE_MAX)
    return GRANIT_ERROR_NOT_READY;
  std::uint64_t slot_generation{};
  {
    std::lock_guard lock{upload_mutex_};
    slot_generation = upload_slots_[slot_index].generation;
  }
  std::unique_ptr<backend_upload_completion> candidate;
  try {
    candidate = std::make_unique<callback_upload_completion>([this, slot_index, slot_generation] {
      return poll_upload_slot(slot_index, slot_generation);
    });
  } catch (const std::bad_alloc&) {
    release_upload_slot(slot_index);
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    release_upload_slot(slot_index);
    return GRANIT_ERROR_INTERNAL;
  }
  auto& context = *upload_slots_[slot_index].context;
  auto finish = [&](granit_result result) {
    release_upload_slot(slot_index);
    return observe_device_result(result);
  };
  auto result = context.ensure_capacity(memory_allocator_, required);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  VkDeviceSize source_offset{};
  for (const auto& upload : uploads) {
    source_offset = (source_offset + alignment - 1) & ~(alignment - 1);
    std::memcpy(static_cast<std::byte*>(context.staging().mapped_data) + source_offset, upload.data,
                static_cast<std::size_t>(upload.size));
    source_offset += upload.size;
  }
  result = memory_allocator_.flush(context.staging(), 0, required);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.begin(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  const auto& functions = device_.functions();
  std::vector<vulkan_image_access> pending_states;
  try {
    pending_states.reserve(uploads.size());
  } catch (const std::bad_alloc&) {
    return finish(GRANIT_ERROR_OUT_OF_MEMORY);
  }
  {
    std::lock_guard queue_lock{queue_mutex_};
    try {
      image_states_.reserve(image_states_.size() + uploads.size());
    } catch (const std::bad_alloc&) {
      return finish(GRANIT_ERROR_OUT_OF_MEMORY);
    }
    source_offset = 0;
    for (const auto& upload : uploads) {
      source_offset = (source_offset + alignment - 1) & ~(alignment - 1);
      if (upload.type == backend_upload_type::buffer) {
        const auto& buffer = static_cast<const vulkan_buffer_resource&>(*upload.buffer).native();
        const VkBufferCopy copy{.srcOffset = source_offset,
                                .dstOffset = upload.destination_offset,
                                .size = upload.size};
        functions.vkCmdCopyBuffer(context.command_buffer(), context.staging().buffer, buffer.buffer,
                                  1, &copy);
      } else {
        const auto& texture = static_cast<const vulkan_texture_resource&>(*upload.texture).native();
        VkBufferImageCopy texture_copy{};
        texture_copy.bufferRowLength = upload.texture_copy.buffer_row_length;
        texture_copy.bufferImageHeight = upload.texture_copy.buffer_image_height;
        texture_copy.imageSubresource = {
            map_texture_aspect(upload.texture_copy.aspect), upload.texture_copy.mip_level,
            upload.texture_copy.base_array_layer, upload.texture_copy.array_layer_count};
        texture_copy.imageOffset = {upload.texture_copy.x, upload.texture_copy.y,
                                    upload.texture_copy.z};
        texture_copy.imageExtent = {upload.texture_copy.width, upload.texture_copy.height,
                                    upload.texture_copy.depth};
        const vulkan_image_access destination{
            .image = texture.image,
            .range = {texture_copy.imageSubresource.aspectMask,
                      texture_copy.imageSubresource.mipLevel, 1,
                      texture_copy.imageSubresource.baseArrayLayer, 1}};
        const auto pending = find_image_subresource(pending_states, destination);
        const auto previous = pending != pending_states.end()
                                  ? pending
                                  : find_image_subresource(image_states_, destination);
        const bool previous_is_pending = pending != pending_states.end();
        const bool has_previous = previous_is_pending || previous != image_states_.end();
        VkImageMemoryBarrier2 barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcStageMask =
            has_previous ? previous->stages : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
        barrier.srcAccessMask = has_previous ? previous->access : 0;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        barrier.oldLayout = has_previous ? previous->layout : VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture.image;
        barrier.subresourceRange = {
            texture_copy.imageSubresource.aspectMask, texture_copy.imageSubresource.mipLevel, 1,
            texture_copy.imageSubresource.baseArrayLayer, texture_copy.imageSubresource.layerCount};
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        functions.vkCmdPipelineBarrier2(context.command_buffer(), &dependency);
        auto copy = texture_copy;
        copy.bufferOffset = source_offset;
        functions.vkCmdCopyBufferToImage(context.command_buffer(), context.staging().buffer,
                                         texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                         &copy);
        vulkan_image_access state{.image = texture.image,
                                  .range = {texture_copy.imageSubresource.aspectMask,
                                            texture_copy.imageSubresource.mipLevel, 1,
                                            texture_copy.imageSubresource.baseArrayLayer,
                                            texture_copy.imageSubresource.layerCount},
                                  .layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                  .stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                                  .access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                                  .preserve_content = true};
        store_unit_image_accesses(pending_states, state);
      }
      source_offset += upload.size;
    }
    result = context.end(device_);
    if (result == GRANIT_SUCCESS)
      result = context.reset_fence(device_);
    if (result != GRANIT_SUCCESS)
      return finish(result);
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = context.command_buffer();
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    result = map_vulkan_result(
        functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, context.fence()));
    if (result == GRANIT_SUCCESS) {
      for (const auto& state : pending_states)
        store_unit_image_accesses(image_states_, state);
    }
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  mark_upload_slot_submitted(slot_index);
  completion = std::move(candidate);
  return GRANIT_SUCCESS;
}

granit_result
vulkan_renderer_state::upload_batch(std::span<const backend_upload_operation> uploads) noexcept {
  std::unique_ptr<backend_upload_completion> completion;
  auto result = upload_batch_async(uploads, completion);
  if (result != GRANIT_SUCCESS)
    return result;
  do {
    result = completion->poll();
    if (result == GRANIT_ERROR_NOT_READY)
      std::this_thread::yield();
  } while (result == GRANIT_ERROR_NOT_READY);
  return result;
}

granit_result vulkan_renderer_state::readback_batch_async(
    std::span<const backend_readback_operation> readbacks, granit_readback_layout, std::uint64_t,
    std::unique_ptr<backend_readback_completion>& completion) noexcept {
  completion.reset();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  if (readbacks.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto alignment =
      std::max<VkDeviceSize>(4, device_.properties().limits.optimalBufferCopyOffsetAlignment);
  VkDeviceSize required{};
  std::vector<VkDeviceSize> offsets;
  std::vector<VkDeviceSize> sizes;
  std::vector<granit_readback_result_info> infos;
  try {
    offsets.reserve(readbacks.size());
    sizes.reserve(readbacks.size());
    infos.reserve(readbacks.size());
    for (const auto& readback : readbacks) {
      if (readback.size == 0 ||
          (readback.type == backend_readback_type::buffer && readback.buffer == nullptr) ||
          (readback.type == backend_readback_type::texture && readback.texture == nullptr))
        return GRANIT_ERROR_INVALID_ARGUMENT;
      const auto offset = (required + alignment - 1) & ~(alignment - 1);
      if (offset < required || readback.size > UINT64_MAX - offset)
        return GRANIT_ERROR_OUT_OF_MEMORY;
      offsets.push_back(offset);
      sizes.push_back(readback.size);
      infos.push_back(readback.result_info);
      required = offset + readback.size;
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }

  const auto slot_index = try_acquire_readback_slot();
  if (slot_index == SIZE_MAX)
    return GRANIT_ERROR_NOT_READY;
  std::uint64_t slot_generation{};
  {
    std::lock_guard lock{readback_mutex_};
    slot_generation = readback_slots_[slot_index].generation;
  }
  auto& context = *readback_slots_[slot_index].context;
  auto finish = [&](granit_result result) {
    release_readback_slot(slot_index, slot_generation);
    return observe_device_result(result);
  };
  auto result = context.ensure_capacity(memory_allocator_, required);
  if (result != GRANIT_SUCCESS)
    return finish(result);
  result = context.begin(device_);
  if (result != GRANIT_SUCCESS)
    return finish(result);

  const auto& functions = device_.functions();
  std::vector<vulkan_image_access> pending_states;
  try {
    pending_states.reserve(readbacks.size());
  } catch (const std::bad_alloc&) {
    return finish(GRANIT_ERROR_OUT_OF_MEMORY);
  }
  {
    std::lock_guard queue_lock{queue_mutex_};
    try {
      image_states_.reserve(image_states_.size() + readbacks.size());
    } catch (const std::bad_alloc&) {
      return finish(GRANIT_ERROR_OUT_OF_MEMORY);
    }
    for (std::size_t index = 0; index < readbacks.size(); ++index) {
      const auto& readback = readbacks[index];
      if (readback.type == backend_readback_type::buffer) {
        const auto& buffer = static_cast<const vulkan_buffer_resource&>(*readback.buffer).native();
        const VkBufferCopy copy{.srcOffset = readback.source_offset,
                                .dstOffset = offsets[index],
                                .size = readback.size};
        functions.vkCmdCopyBuffer(context.command_buffer(), buffer.buffer, context.staging().buffer,
                                  1, &copy);
        continue;
      }
      const auto& texture = static_cast<const vulkan_texture_resource&>(*readback.texture).native();
      const auto& region = readback.texture_region;
      VkBufferImageCopy copy{};
      copy.bufferOffset = offsets[index];
      copy.bufferRowLength = 0;
      copy.bufferImageHeight = 0;
      copy.imageSubresource = {map_texture_aspect(region.aspect), region.mip_level,
                               region.base_array_layer, region.array_layer_count};
      copy.imageOffset = {static_cast<std::int32_t>(region.x), static_cast<std::int32_t>(region.y),
                          static_cast<std::int32_t>(region.z)};
      copy.imageExtent = {region.width, region.height, region.depth};
      const vulkan_image_access source{
          .image = texture.image,
          .range = {copy.imageSubresource.aspectMask, copy.imageSubresource.mipLevel, 1,
                    copy.imageSubresource.baseArrayLayer, copy.imageSubresource.layerCount}};
      const auto pending = find_image_subresource(pending_states, source);
      const auto previous =
          pending != pending_states.end() ? pending : find_image_subresource(image_states_, source);
      const bool has_previous = pending != pending_states.end() || previous != image_states_.end();
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = has_previous ? previous->stages : VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
      barrier.srcAccessMask = has_previous ? previous->access : 0;
      barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
      barrier.oldLayout = has_previous ? previous->layout : VK_IMAGE_LAYOUT_UNDEFINED;
      barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = texture.image;
      barrier.subresourceRange = source.range;
      VkDependencyInfo dependency{};
      dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
      dependency.imageMemoryBarrierCount = 1;
      dependency.pImageMemoryBarriers = &barrier;
      functions.vkCmdPipelineBarrier2(context.command_buffer(), &dependency);
      functions.vkCmdCopyImageToBuffer(context.command_buffer(), texture.image,
                                       VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                       context.staging().buffer, 1, &copy);
      auto state = source;
      state.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
      state.stages = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
      state.access = VK_ACCESS_2_TRANSFER_READ_BIT;
      state.preserve_content = true;
      store_unit_image_accesses(pending_states, state);
    }
    result = context.end(device_);
    if (result == GRANIT_SUCCESS)
      result = context.reset_fence(device_);
    if (result != GRANIT_SUCCESS)
      return finish(result);
    VkCommandBufferSubmitInfo command_info{};
    command_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_info.commandBuffer = context.command_buffer();
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount = 1;
    submit_info.pCommandBufferInfos = &command_info;
    result = map_vulkan_result(
        functions.vkQueueSubmit2(device_.graphics_queue(), 1, &submit_info, context.fence()));
    if (result == GRANIT_SUCCESS) {
      for (const auto& state : pending_states)
        store_unit_image_accesses(image_states_, state);
    }
  }
  if (result != GRANIT_SUCCESS) {
    static_cast<void>(context.restore_signaled_fence(device_));
    return finish(result);
  }
  try {
    completion = std::make_unique<vulkan_readback_completion>(
        [this, slot_index, slot_generation] {
          return poll_readback_slot(slot_index, slot_generation);
        },
        [infos = std::move(infos)](std::uint32_t index, granit_readback_result_info& info) {
          if (index >= infos.size())
            return GRANIT_ERROR_INVALID_ARGUMENT;
          info = infos[index];
          return GRANIT_SUCCESS;
        },
        [this, slot_index, slot_generation, offsets = std::move(offsets),
         sizes = std::move(sizes)](std::uint32_t index, void* data, std::uint64_t size) {
          std::lock_guard lock{readback_mutex_};
          if (slot_index >= readback_slots_.size() ||
              readback_slots_[slot_index].generation != slot_generation ||
              index >= offsets.size() || data == nullptr || size != sizes[index])
            return GRANIT_ERROR_INVALID_ARGUMENT;
          auto& staging = readback_slots_[slot_index].context->staging();
          auto invalidate_result = memory_allocator_.invalidate(staging, offsets[index], size);
          if (invalidate_result != GRANIT_SUCCESS)
            return observe_device_result(invalidate_result);
          std::memcpy(data, static_cast<const std::byte*>(staging.mapped_data) + offsets[index],
                      static_cast<std::size_t>(size));
          return GRANIT_SUCCESS;
        },
        [this, slot_index, slot_generation] {
          release_readback_slot(slot_index, slot_generation);
        });
  } catch (const std::bad_alloc&) {
    return finish(GRANIT_ERROR_OUT_OF_MEMORY);
  } catch (...) {
    return finish(GRANIT_ERROR_INTERNAL);
  }
  mark_readback_slot_submitted(slot_index);
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
