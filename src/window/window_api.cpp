// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/window/window.h>

#include "window/platform/backend.h"

#include <cstring>
#include <utility>

using namespace granit::window::detail;

namespace {

constexpr std::uint32_t maximum_target_value_length = 4096;

const granit_window_target_desc* get_window_target(const granit_window_desc& desc) noexcept {
  return desc.struct_size >= GRANIT_WINDOW_DESC_VERSION_2_SIZE ? desc.target : nullptr;
}
granit_result validate_window_target(const granit_window_desc& desc) noexcept {
  const auto* target = get_window_target(desc);
  if (target == nullptr)
    return GRANIT_SUCCESS;
  if (target->struct_size < GRANIT_WINDOW_TARGET_DESC_VERSION_1_SIZE || target->flags != 0 ||
      target->reserved != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  if (target->type == GRANIT_WINDOW_TARGET_AUTOMATIC)
    return target->value == nullptr && target->value_length == 0 ? GRANIT_SUCCESS
                                                                 : GRANIT_ERROR_INVALID_ARGUMENT;
  if (target->type != GRANIT_WINDOW_TARGET_CANVAS_SELECTOR || target->value == nullptr ||
      target->value_length == 0 || target->value_length > maximum_target_value_length ||
      std::memchr(target->value, '\0', target->value_length) != nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  return GRANIT_SUCCESS;
}

} // namespace

extern "C" granit_result granit_window_create(granit_window_system system_handle,
                                              const granit_window_desc* desc,
                                              granit_window* output) {
  if (output == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *output = GRANIT_NULL_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_WINDOW_DESC_VERSION_1_SIZE ||
      desc->width == 0 || desc->height == 0 || desc->reserved != 0 ||
      (desc->flags & ~(GRANIT_WINDOW_FLAG_VISIBLE_BIT | GRANIT_WINDOW_FLAG_RESIZABLE_BIT |
                       GRANIT_WINDOW_FLAG_HIGH_DPI_BIT)) != 0)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto target_result = validate_window_target(*desc);
  if (target_result != GRANIT_SUCCESS)
    return target_result;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto* target = get_window_target(*desc);
  if (target != nullptr && target->type == GRANIT_WINDOW_TARGET_CANVAS_SELECTOR &&
      (system->operations == nullptr || !system->operations->accepts_canvas_target)) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  return system->operations == nullptr ? GRANIT_ERROR_INTERNAL
                                       : system->operations->create_window(system, desc, output);
}

extern "C" granit_result granit_window_destroy(granit_window_system system_handle,
                                               granit_window window_handle) {
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  clear_window_input(*system, window_handle);
  return system->operations == nullptr ? GRANIT_ERROR_INTERNAL
                                       : system->operations->destroy_window(system, window_handle);
}

extern "C" granit_result granit_window_get_state(granit_window_system system_handle,
                                                 granit_window window_handle,
                                                 granit_window_state* state) {
  if (state == nullptr || state->struct_size < GRANIT_WINDOW_STATE_VERSION_1_SIZE)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *state = GRANIT_WINDOW_STATE_INIT;
  auto system = acquire_system(system_handle);
  if (!system)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (!on_owner_thread(*system))
    return GRANIT_ERROR_INVALID_ARGUMENT;
  const auto found = system->windows.find(window_handle);
  if (found == system->windows.end())
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto& window = *found->second;
  state->width = window.width;
  state->height = window.height;
  state->framebuffer_width = window.framebuffer_width;
  state->framebuffer_height = window.framebuffer_height;
  state->content_scale_horizontal = window.content_scale_horizontal;
  state->content_scale_vertical = window.content_scale_vertical;
  return GRANIT_SUCCESS;
}

