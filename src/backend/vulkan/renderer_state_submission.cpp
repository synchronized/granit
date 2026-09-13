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

granit_result vulkan_renderer_state::complete_frame_slot(frame_slot& slot) noexcept {
  if (slot.serial == 0) {
    return GRANIT_SUCCESS;
  }
  const auto result = slot.context->wait(device_, UINT64_MAX);
  if (result != GRANIT_SUCCESS) {
    return observe_device_result(result);
  }
  for (auto* recorder : slot.recorders)
    recorder->mark_complete();
  submission_serials_.mark_completed(slot.serial);
  slot.recorders.clear();
  slot.serial = 0;
  return GRANIT_SUCCESS;
}

granit_result
vulkan_renderer_state::submit_command_recorder(backend_command_recorder_resource& resource,
                                               submission_serial& submitted_serial) {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::executable || frame_slots_.empty())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = complete_frame_slot(slot);
  if (result != GRANIT_SUCCESS)
    return result;
  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  std::vector<VkImageMemoryBarrier2> image_barriers;
  image_barriers.reserve(recorder.initial_image_accesses().size());
  image_states_.reserve(image_states_.size() + recorder.final_image_accesses().size());
  for (const auto& destination : recorder.initial_image_accesses()) {
    const auto previous =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& state) { return same_image_subresource(state, destination); });
    if (previous == image_states_.end() && destination.preserve_content)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask =
        previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
    barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
    barrier.dstStageMask = destination.stages;
    barrier.dstAccessMask = destination.access;
    barrier.oldLayout =
        previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
    barrier.newLayout = destination.layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = destination.image;
    barrier.subresourceRange = destination.range;
    image_barriers.push_back(barrier);
  }
  const bool use_preamble = !image_barriers.empty();
  if (use_preamble) {
    if (slot.preamble->state() == command_recorder_state::executable) {
      result = slot.preamble->reset(device_);
      if (result != GRANIT_SUCCESS)
        return observe_device_result(result);
    }
    result = slot.preamble->begin(device_);
    if (result == GRANIT_SUCCESS)
      result = slot.preamble->record_image_barriers(device_, image_barriers);
    if (result == GRANIT_SUCCESS)
      result = slot.preamble->end(device_);
    if (result != GRANIT_SUCCESS)
      return observe_device_result(result);
  }
  slot.recorders.clear();
  slot.recorders.push_back(&recorder);
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS) {
    slot.recorders.clear();
    return observe_device_result(result);
  }
  std::array<VkCommandBufferSubmitInfo, 2> command_infos{};
  std::uint32_t command_count{};
  if (use_preamble) {
    command_infos[command_count].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_infos[command_count++].commandBuffer = slot.preamble->native_handle();
  }
  command_infos[command_count].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command_infos[command_count++].commandBuffer = recorder.native_handle();
  VkSubmitInfo2 submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit_info.commandBufferInfoCount = command_count;
  submit_info.pCommandBufferInfos = command_infos.data();
  const auto submit_result = device_.functions().vkQueueSubmit2(
      device_.graphics_queue(), 1, &submit_info, slot.context->completion_fence());
  if (submit_result != VK_SUCCESS) {
    slot.recorders.clear();
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(submit_result));
  }
  static_cast<void>(recorder.mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  for (const auto& final : recorder.final_image_accesses()) {
    const auto state =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& current) { return same_image_subresource(current, final); });
    if (state == image_states_.end())
      image_states_.push_back(final);
    else
      *state = final;
  }
  slot.serial = serial;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result vulkan_renderer_state::submit_command_recorders(
    std::span<backend_command_recorder_resource* const> resources,
    submission_serial& submitted_serial) {
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::vector<vulkan_command_recorder*> recorders;
  try {
    recorders.reserve(resources.size());
    for (auto* resource : resources) {
      if (resource == nullptr)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      recorders.push_back(&static_cast<vulkan_command_recorder_resource&>(*resource).native());
    }
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  }
  std::lock_guard lock{queue_mutex_};
  if (recorders.empty() || frame_slots_.empty()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  for (const auto* recorder : recorders) {
    if (recorder == nullptr || recorder->state() != command_recorder_state::executable)
      return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto result = complete_frame_slot(slot);
  if (result != GRANIT_SUCCESS) {
    return result;
  }
  const auto serial = submission_serials_.next();
  if (serial == 0) {
    return GRANIT_ERROR_INTERNAL;
  }
  while (slot.batch_preambles.size() < recorders.size()) {
    auto preamble = std::make_unique<vulkan_command_recorder>();
    result = preamble->initialize(device_);
    if (result != GRANIT_SUCCESS)
      return observe_device_result(result);
    slot.batch_preambles.push_back(std::move(preamble));
  }
  auto next_image_states = image_states_;
  std::size_t final_access_count{};
  for (const auto* recorder : recorders)
    final_access_count += recorder->final_image_accesses().size();
  next_image_states.reserve(next_image_states.size() + final_access_count);
  std::vector<VkCommandBufferSubmitInfo> command_infos;
  command_infos.reserve(recorders.size() * 2);
  std::vector<VkSubmitInfo2> submit_infos;
  submit_infos.reserve(recorders.size());
  for (std::size_t recorder_index = 0; recorder_index < recorders.size(); ++recorder_index) {
    auto& recorder = *recorders[recorder_index];
    std::vector<VkImageMemoryBarrier2> image_barriers;
    image_barriers.reserve(recorder.initial_image_accesses().size());
    for (const auto& destination : recorder.initial_image_accesses()) {
      const auto previous =
          std::find_if(next_image_states.begin(), next_image_states.end(), [&](const auto& state) {
            return same_image_subresource(state, destination);
          });
      if (previous == next_image_states.end() && destination.preserve_content)
        return GRANIT_ERROR_INVALID_ARGUMENT;
      VkImageMemoryBarrier2 barrier{};
      barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
      barrier.srcStageMask = previous == next_image_states.end()
                                 ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT
                                 : previous->stages;
      barrier.srcAccessMask = previous == next_image_states.end() ? 0 : previous->access;
      barrier.dstStageMask = destination.stages;
      barrier.dstAccessMask = destination.access;
      barrier.oldLayout =
          previous == next_image_states.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
      barrier.newLayout = destination.layout;
      barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
      barrier.image = destination.image;
      barrier.subresourceRange = destination.range;
      image_barriers.push_back(barrier);
    }
    const auto first_command = command_infos.size();
    if (!image_barriers.empty()) {
      auto& preamble = *slot.batch_preambles[recorder_index];
      if (preamble.state() == command_recorder_state::executable) {
        result = preamble.reset(device_);
        if (result != GRANIT_SUCCESS)
          return observe_device_result(result);
      }
      result = preamble.begin(device_);
      if (result == GRANIT_SUCCESS)
        result = preamble.record_image_barriers(device_, image_barriers);
      if (result == GRANIT_SUCCESS)
        result = preamble.end(device_);
      if (result != GRANIT_SUCCESS)
        return observe_device_result(result);
      VkCommandBufferSubmitInfo preamble_info{};
      preamble_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
      preamble_info.commandBuffer = preamble.native_handle();
      command_infos.push_back(preamble_info);
    }
    VkCommandBufferSubmitInfo recorder_info{};
    recorder_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    recorder_info.commandBuffer = recorder.native_handle();
    command_infos.push_back(recorder_info);
    VkSubmitInfo2 submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit_info.commandBufferInfoCount =
        static_cast<std::uint32_t>(command_infos.size() - first_command);
    submit_info.pCommandBufferInfos = command_infos.data() + first_command;
    submit_infos.push_back(submit_info);
    for (const auto& final : recorder.final_image_accesses()) {
      const auto state =
          std::find_if(next_image_states.begin(), next_image_states.end(),
                       [&](const auto& current) { return same_image_subresource(current, final); });
      if (state == next_image_states.end())
        next_image_states.push_back(final);
      else
        *state = final;
    }
  }
  slot.recorders.assign(recorders.begin(), recorders.end());
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS) {
    slot.recorders.clear();
    return observe_device_result(result);
  }
  const auto submit_result = device_.functions().vkQueueSubmit2(
      device_.graphics_queue(), static_cast<std::uint32_t>(submit_infos.size()),
      submit_infos.data(), slot.context->completion_fence());
  if (submit_result != VK_SUCCESS) {
    slot.recorders.clear();
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(submit_result));
  }
  for (auto* recorder : recorders)
    static_cast<void>(recorder->mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  image_states_ = std::move(next_image_states);
  slot.serial = serial;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result
vulkan_renderer_state::wait_command_recorder(backend_command_recorder_resource& resource) noexcept {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(resource).native();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (recorder.state() != command_recorder_state::pending) {
    return GRANIT_SUCCESS;
  }
  for (auto& slot : frame_slots_) {
    if (std::find(slot.recorders.begin(), slot.recorders.end(), &recorder) !=
        slot.recorders.end()) {
      return complete_frame_slot(slot);
    }
  }
  return GRANIT_ERROR_INTERNAL;
}

granit_result vulkan_renderer_state::wait_for_all_submissions() noexcept {
  std::lock_guard lock{queue_mutex_};
  for (auto& slot : frame_slots_) {
    const auto result = complete_frame_slot(slot);
    if (result != GRANIT_SUCCESS) {
      return result;
    }
  }
  return GRANIT_SUCCESS;
}

void vulkan_renderer_state::retire_resource(submission_serial retire_after, retirement_order order,
                                            std::shared_ptr<void> resource) {
  std::lock_guard lock{retirement_mutex_};
  retirement_queue_.retire(retire_after, order, std::move(resource));
}

std::size_t vulkan_renderer_state::collect_retired() noexcept {
  submission_serial completed{};
  {
    std::lock_guard lock{queue_mutex_};
    completed = submission_serials_.completed();
  }
  std::lock_guard lock{retirement_mutex_};
  return retirement_queue_.collect(completed);
}

std::size_t vulkan_renderer_state::pending_retirement_count() const noexcept {
  std::lock_guard lock{retirement_mutex_};
  return retirement_queue_.size();
}

std::size_t vulkan_renderer_state::drain_retired() noexcept {
  std::lock_guard lock{retirement_mutex_};
  return retirement_queue_.drain();
}

void vulkan_renderer_state::destroy_native_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  static_cast<vulkan_command_recorder_resource&>(resource).native().destroy(device_);
}

granit_result vulkan_renderer_state::discard_command_recorder(
    backend_command_recorder_resource& resource) noexcept {
  destroy_native_command_recorder(resource);
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
