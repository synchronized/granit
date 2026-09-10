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

std::unique_ptr<backend_surface_resource> vulkan_renderer_state::allocate_surface_resource() {
  return std::make_unique<vulkan_surface_resource>(shared_from_this());
}

std::unique_ptr<backend_swapchain_resource> vulkan_renderer_state::allocate_swapchain_resource() {
  return std::make_unique<vulkan_swapchain_resource>(shared_from_this());
}

granit_result
vulkan_renderer_state::create_win32_surface(void* native_instance, void* native_window,
                                            backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WIN32_BIT) == 0) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_win32_surface(instance_, device_, native_instance, native_window, surface));
}

granit_result
vulkan_renderer_state::create_xcb_surface(void* connection, std::uint32_t window,
                                          backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_XCB_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_xcb_surface(instance_, device_, connection, window, surface));
}

granit_result
vulkan_renderer_state::create_wayland_surface(void* display, void* native_surface,
                                              backend_surface_resource& surface_resource) noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{resource_mutex_};
  if ((surface_types_ & GRANIT_SURFACE_TYPE_WAYLAND_BIT) == 0)
    return GRANIT_ERROR_UNSUPPORTED;
  auto& surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  return observe_device_result(
      detail::create_wayland_surface(instance_, device_, display, native_surface, surface));
}

granit_result vulkan_renderer_state::create_canvas_surface(std::string_view,
                                                           backend_surface_resource&) noexcept {
  return device_lost() ? GRANIT_ERROR_DEVICE_LOST : GRANIT_ERROR_UNSUPPORTED;
}

void vulkan_renderer_state::destroy_native_surface(VkSurfaceKHR surface) noexcept {
  std::lock_guard lock{resource_mutex_};
  detail::destroy_surface(instance_, surface);
}

granit_result
vulkan_renderer_state::create_swapchain(backend_surface_resource& surface_resource,
                                        const backend_swapchain_desc& desc,
                                        backend_swapchain_resource& swapchain_resource) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
  const vulkan_swapchain_desc native_desc{desc.width, desc.height, desc.minimum_image_count,
                                          desc.present_mode};
  std::lock_guard lock{resource_mutex_};
  return observe_device_result(swapchain.initialize(instance_, device_, surface, native_desc));
}

granit_result
vulkan_renderer_state::recreate_swapchain(backend_surface_resource& surface_resource,
                                          const backend_swapchain_desc& desc,
                                          backend_swapchain_resource& swapchain_resource) {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  const auto surface = static_cast<vulkan_surface_resource&>(surface_resource).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
  const vulkan_swapchain_desc native_desc{desc.width, desc.height, desc.minimum_image_count,
                                          desc.present_mode};
  std::lock_guard lock{resource_mutex_};
  {
    std::lock_guard queue_lock{queue_mutex_};
    for (const auto image : swapchain.images()) {
      std::erase_if(image_states_, [&](const auto& state) { return state.image == image; });
    }
  }
  return observe_device_result(swapchain.recreate(instance_, device_, surface, native_desc));
}

backend_swapchain_info
vulkan_renderer_state::get_swapchain_info(backend_swapchain_resource& swapchain_resource) noexcept {
  std::lock_guard lock{resource_mutex_};
  const auto info = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native().info();
  return {info.width, info.height, info.image_count, info.present_mode,
          map_swapchain_format(info.format)};
}

granit_result vulkan_renderer_state::get_swapchain_backbuffers(
    backend_swapchain_resource& swapchain_resource,
    std::vector<backend_swapchain_backbuffer>& backbuffers) {
  try {
    std::lock_guard lock{resource_mutex_};
    auto& swapchain = static_cast<vulkan_swapchain_resource&>(swapchain_resource).native();
    const auto info = swapchain.info();
    backbuffers.clear();
    backbuffers.reserve(swapchain.images().size());
    for (const auto image : swapchain.images()) {
      auto texture = std::make_unique<vulkan_texture_resource>(shared_from_this(), false);
      texture->native().image = image;
      granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
      desc.format = map_swapchain_format(info.format);
      desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
      desc.width = info.width;
      desc.height = info.height;
      backbuffers.push_back({std::move(texture), nullptr, desc});
    }
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result
vulkan_renderer_state::prepare_swapchain_backbuffer(backend_swapchain_backbuffer& backbuffer) {
  if (!backbuffer.texture)
    return GRANIT_ERROR_INTERNAL;
  if (backbuffer.view)
    return GRANIT_SUCCESS;
  backbuffer.view = allocate_texture_view_resource();
  if (!backbuffer.view)
    return GRANIT_ERROR_OUT_OF_MEMORY;
  granit_texture_view_desc desc = GRANIT_TEXTURE_VIEW_DESC_INIT;
  return create_native_texture_view(*backbuffer.texture, backbuffer.desc, desc, *backbuffer.view);
}

void vulkan_renderer_state::destroy_native_swapchain(vulkan_swapchain& swapchain) noexcept {
  std::lock_guard lock{resource_mutex_};
  {
    std::lock_guard queue_lock{queue_mutex_};
    for (const auto image : swapchain.images()) {
      std::erase_if(image_states_, [&](const auto& state) { return state.image == image; });
    }
  }
  swapchain.reset(device_);
}

granit_result
vulkan_renderer_state::acquire_swapchain_frame(backend_swapchain_resource& resource,
                                               backend_acquired_swapchain_frame& frame) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  auto& slot = frame_slots_[next_frame_slot_];
  if (slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto complete_result = complete_frame_slot(slot);
  if (complete_result != GRANIT_SUCCESS)
    return complete_result;
  const auto acquired = swapchain.acquire(device_, slot.context->image_available());
  if (acquired.result != GRANIT_SUCCESS)
    return observe_device_result(acquired.result);
  slot.acquired = true;
  frame.image_index = acquired.image_index;
  frame.slot_index = next_frame_slot_;
  frame.needs_recreate = acquired.suboptimal;
  frame.dynamic_backbuffer = {};
  return GRANIT_SUCCESS;
}

granit_result vulkan_renderer_state::submit_swapchain_frame(
    backend_command_recorder_resource& command, backend_swapchain_resource& resource,
    std::uint32_t image_index, std::size_t slot_index, submission_serial& submitted_serial) {
  auto& recorder = static_cast<vulkan_command_recorder_resource&>(command).native();
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
  submitted_serial = 0;
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size() || image_index >= swapchain.images().size() ||
      recorder.state() != command_recorder_state::executable)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.acquired || slot.awaiting_present || slot_index != next_frame_slot_)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto image = swapchain.images()[image_index];
  const auto final =
      std::find_if(recorder.final_image_accesses().begin(), recorder.final_image_accesses().end(),
                   [&](const auto& access) { return access.image == image; });
  if (final == recorder.final_image_accesses().end())
    return GRANIT_ERROR_INVALID_ARGUMENT;

  std::vector<VkImageMemoryBarrier2> initial_barriers;
  initial_barriers.reserve(recorder.initial_image_accesses().size());
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
    initial_barriers.push_back(barrier);
  }
  auto result = slot.preamble->state() == command_recorder_state::executable
                    ? slot.preamble->reset(device_)
                    : GRANIT_SUCCESS;
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->begin(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->record_image_barriers(device_, initial_barriers);
  if (result == GRANIT_SUCCESS)
    result = slot.preamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);

  if (slot.postamble->state() == command_recorder_state::executable)
    result = slot.postamble->reset(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->begin(device_);
  VkImageMemoryBarrier2 present_barrier{};
  present_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  present_barrier.srcStageMask = final->stages;
  present_barrier.srcAccessMask = final->access;
  present_barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  present_barrier.dstAccessMask = 0;
  present_barrier.oldLayout = final->layout;
  present_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  present_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  present_barrier.image = image;
  present_barrier.subresourceRange = final->range;
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->record_image_barriers(device_, {&present_barrier, 1});
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);

  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);
  std::array<VkCommandBufferSubmitInfo, 3> commands{};
  const VkCommandBuffer buffers[]{slot.preamble->native_handle(), recorder.native_handle(),
                                  slot.postamble->native_handle()};
  for (std::size_t index = 0; index < commands.size(); ++index) {
    commands[index].sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    commands[index].commandBuffer = buffers[index];
  }
  VkSemaphoreSubmitInfo wait{};
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = slot.context->image_available();
  wait.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSemaphoreSubmitInfo signal{};
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = swapchain.render_finished(image_index);
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = static_cast<std::uint32_t>(commands.size());
  submit.pCommandBufferInfos = commands.data();
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  const auto queue_result = device_.functions().vkQueueSubmit2(device_.graphics_queue(), 1, &submit,
                                                               slot.context->completion_fence());
  if (queue_result != VK_SUCCESS) {
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(queue_result));
  }
  static_cast<void>(recorder.mark_pending());
  static_cast<void>(submission_serials_.commit(serial));
  submitted_serial = serial;
  for (const auto& access : recorder.final_image_accesses()) {
    const auto state =
        std::find_if(image_states_.begin(), image_states_.end(),
                     [&](const auto& current) { return same_image_subresource(current, access); });
    if (state == image_states_.end())
      image_states_.push_back(access);
    else
      *state = access;
  }
  auto present_state = *final;
  present_state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  present_state.stages = VK_PIPELINE_STAGE_2_NONE;
  present_state.access = 0;
  const auto state =
      std::find_if(image_states_.begin(), image_states_.end(), [&](const auto& current) {
        return same_image_subresource(current, present_state);
      });
  *state = present_state;
  slot.recorders.clear();
  slot.recorders.push_back(&recorder);
  slot.serial = serial;
  slot.acquired = false;
  slot.awaiting_present = true;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return GRANIT_SUCCESS;
}

granit_result vulkan_renderer_state::present_swapchain_frame(backend_swapchain_resource& resource,
                                                             std::uint32_t image_index,
                                                             std::size_t slot_index,
                                                             bool& needs_recreate) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto presented = swapchain.present(device_, device_.graphics_queue(), image_index,
                                           swapchain.render_finished(image_index));
  slot.awaiting_present = false;
  needs_recreate = presented.suboptimal || presented.result == GRANIT_ERROR_OUT_OF_DATE;
  return observe_device_result(presented.result);
}

granit_result vulkan_renderer_state::cancel_swapchain_frame(backend_swapchain_resource& resource,
                                                            std::uint32_t image_index,
                                                            std::size_t slot_index,
                                                            bool& needs_recreate) {
  auto& swapchain = static_cast<vulkan_swapchain_resource&>(resource).native();
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  if (slot_index >= frame_slots_.size() || image_index >= swapchain.images().size())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  auto& slot = frame_slots_[slot_index];
  if (!slot.acquired || slot.awaiting_present)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto image = swapchain.images()[image_index];
  image_states_.reserve(image_states_.size() + 1);
  const auto previous = std::find_if(image_states_.begin(), image_states_.end(),
                                     [&](const auto& state) { return state.image == image; });
  VkImageMemoryBarrier2 barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
  barrier.srcStageMask =
      previous == image_states_.end() ? VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT : previous->stages;
  barrier.srcAccessMask = previous == image_states_.end() ? 0 : previous->access;
  barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
  barrier.oldLayout =
      previous == image_states_.end() ? VK_IMAGE_LAYOUT_UNDEFINED : previous->layout;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                              .baseMipLevel = 0,
                              .levelCount = 1,
                              .baseArrayLayer = 0,
                              .layerCount = 1};
  auto result = slot.postamble->state() == command_recorder_state::executable
                    ? slot.postamble->reset(device_)
                    : GRANIT_SUCCESS;
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->begin(device_);
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->record_image_barriers(device_, {&barrier, 1});
  if (result == GRANIT_SUCCESS)
    result = slot.postamble->end(device_);
  if (result != GRANIT_SUCCESS)
    return observe_device_result(result);
  const auto serial = submission_serials_.next();
  if (serial == 0)
    return GRANIT_ERROR_INTERNAL;
  result = slot.context->reset_fence(device_);
  if (result != GRANIT_SUCCESS)
    return result;
  VkSemaphoreSubmitInfo wait{};
  wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  wait.semaphore = slot.context->image_available();
  wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkCommandBufferSubmitInfo command{};
  command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  command.commandBuffer = slot.postamble->native_handle();
  VkSemaphoreSubmitInfo signal{};
  signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
  signal.semaphore = swapchain.render_finished(image_index);
  signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  VkSubmitInfo2 submit{};
  submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submit.waitSemaphoreInfoCount = 1;
  submit.pWaitSemaphoreInfos = &wait;
  submit.commandBufferInfoCount = 1;
  submit.pCommandBufferInfos = &command;
  submit.signalSemaphoreInfoCount = 1;
  submit.pSignalSemaphoreInfos = &signal;
  const auto queue_result = device_.functions().vkQueueSubmit2(device_.graphics_queue(), 1, &submit,
                                                               slot.context->completion_fence());
  if (queue_result != VK_SUCCESS) {
    static_cast<void>(slot.context->restore_signaled_fence(device_));
    return observe_device_result(map_vulkan_result(queue_result));
  }
  static_cast<void>(submission_serials_.commit(serial));
  slot.serial = serial;
  slot.acquired = false;
  slot.awaiting_present = true;
  const auto presented = swapchain.present(device_, device_.graphics_queue(), image_index,
                                           swapchain.render_finished(image_index));
  slot.awaiting_present = false;
  needs_recreate = presented.suboptimal || presented.result == GRANIT_ERROR_OUT_OF_DATE;
  vulkan_image_access present_state{};
  present_state.image = image;
  present_state.range = barrier.subresourceRange;
  present_state.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  if (previous == image_states_.end())
    image_states_.push_back(present_state);
  else
    *previous = present_state;
  next_frame_slot_ = (next_frame_slot_ + 1) % frame_slots_.size();
  return observe_device_result(presented.result);
}

granit_result vulkan_renderer_state::wait_for_present_idle() noexcept {
  if (device_lost())
    return GRANIT_ERROR_DEVICE_LOST;
  std::lock_guard lock{queue_mutex_};
  const auto result = observe_device_result(
      map_vulkan_result(device_.functions().vkQueueWaitIdle(device_.graphics_queue())));
  if (result == GRANIT_SUCCESS)
    submission_serials_.mark_completed(submission_serials_.last_submitted());
  return result;
}

} // namespace granit::detail
