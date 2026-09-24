// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "imgui/imgui_font_atlas.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace granit::example::imgui {

granit::result initialize_font_atlas(granit::renderer& renderer, texture_registry& registry,
                                     granit::texture& texture, granit::texture_view& view,
                                     granit::sampler& sampler) {
  unsigned char* source{};
  int width{};
  int height{};
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&source, &width, &height);
  if (source == nullptr || width <= 0 || height <= 0)
    return granit::result::internal;

  const auto byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  std::vector<std::byte> pixels(byte_count);
  for (std::size_t offset = 0; offset < byte_count; offset += 4) {
    const auto alpha = source[offset + 3];
    for (std::size_t channel = 0; channel < 3; ++channel) {
      pixels[offset + channel] = static_cast<std::byte>(
          (static_cast<std::uint32_t>(source[offset + channel]) * alpha + 127U) / 255U);
    }
    pixels[offset + 3] = static_cast<std::byte>(alpha);
  }

  auto result = texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::sampled |
                                                       granit::texture_usage::transfer_destination,
                                              .width = static_cast<std::uint32_t>(width),
                                              .height = static_cast<std::uint32_t>(height)});
  if (result.ok()) {
    result = texture.write(
        pixels,
        {.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
         .rows_per_image = static_cast<std::uint32_t>(height)},
        {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)});
  }
  if (result.ok())
    result = view.initialize(renderer, texture);
  if (result.ok()) {
    result = sampler.initialize(renderer, {.address_u = granit::address_mode::clamp_to_edge,
                                           .address_v = granit::address_mode::clamp_to_edge,
                                           .address_w = granit::address_mode::clamp_to_edge});
  }
  ImTextureID texture_id = ImTextureID_Invalid;
  if (result.ok())
    result = registry.register_texture(view.ref(), sampler.ref(), texture_id);
  if (result.ok()) {
    ImGui::GetIO().Fonts->SetTexID(texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
  }
  return result;
}

} // namespace granit::example::imgui
