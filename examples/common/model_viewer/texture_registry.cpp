// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/texture_registry.h"

#include <limits>
#include <new>

namespace granit::example::model_viewer {
namespace {

ImTextureID encode(std::uint32_t index, std::uint32_t generation) noexcept {
  return (static_cast<ImTextureID>(generation) << 32U) | (static_cast<ImTextureID>(index) + 1U);
}

bool decode(ImTextureID texture, std::uint32_t& index, std::uint32_t& generation) noexcept {
  const auto encoded_index = static_cast<std::uint32_t>(texture & UINT64_C(0xffffffff));
  generation = static_cast<std::uint32_t>(texture >> 32U);
  if (encoded_index == 0 || generation == 0)
    return false;
  index = encoded_index - 1U;
  return true;
}

} // namespace

void texture_registry::retire(slot& target) noexcept {
  target.view = GRANIT_NULL_HANDLE;
  target.sampler = GRANIT_NULL_HANDLE;
  target.alive = false;
  target.generation =
      target.generation == std::numeric_limits<std::uint32_t>::max() ? 1U : target.generation + 1U;
}

granit::result texture_registry::register_texture(granit_texture_view view, granit_sampler sampler,
                                                  ImTextureID& texture) {
  texture = ImTextureID_Invalid;
  if (view == GRANIT_NULL_HANDLE || sampler == GRANIT_NULL_HANDLE)
    return granit::result::invalid_handle;
  try {
    std::uint32_t index = 0;
    while (index < slots_.size() && slots_[index].alive)
      ++index;
    if (index == slots_.size()) {
      if (slots_.size() >= std::numeric_limits<std::uint32_t>::max())
        return granit::result::out_of_memory;
      slots_.emplace_back();
    }
    auto& target = slots_[index];
    target.view = view;
    target.sampler = sampler;
    target.alive = true;
    texture = encode(index, target.generation);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

granit::result texture_registry::unregister_texture(ImTextureID texture) noexcept {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;
  if (!decode(texture, index, generation) || index >= slots_.size())
    return granit::result::invalid_handle;
  auto& target = slots_[index];
  if (!target.alive || target.generation != generation)
    return granit::result::invalid_handle;
  retire(target);
  return granit::result::success;
}

void texture_registry::clear() noexcept {
  for (auto& target : slots_) {
    if (target.alive)
      retire(target);
  }
}

granit::result texture_registry::resolve(ImTextureID texture,
                                         granit_canvas_draw_state& state) const noexcept {
  std::uint32_t index = 0;
  std::uint32_t generation = 0;
  if (!decode(texture, index, generation) || index >= slots_.size())
    return granit::result::invalid_handle;
  const auto& target = slots_[index];
  if (!target.alive || target.generation != generation)
    return granit::result::invalid_handle;
  state.texture = target.view;
  state.sampler = target.sampler;
  return granit::result::success;
}

granit::result texture_registry::resolver(ImTextureID texture, granit_canvas_draw_state& state,
                                          void* user_data) noexcept {
  if (user_data == nullptr)
    return granit::result::invalid_argument;
  return static_cast<const texture_registry*>(user_data)->resolve(texture, state);
}

} // namespace granit::example::model_viewer
