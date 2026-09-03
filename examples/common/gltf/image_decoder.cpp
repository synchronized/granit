// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/image_decoder.h"

#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstring>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace granit::example::gltf {

image_decode_error decode_image(std::span<const std::byte> encoded, image& output) {
  if (encoded.empty())
    return image_decode_error::invalid_data;
  if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
    return image_decode_error::numeric_overflow;
  try {
    int width{};
    int height{};
    int source_channels{};
    using pixels_owner = std::unique_ptr<stbi_uc, decltype(&stbi_image_free)>;
    pixels_owner pixels(stbi_load_from_memory(reinterpret_cast<const stbi_uc*>(encoded.data()),
                                              static_cast<int>(encoded.size()), &width, &height,
                                              &source_channels, 4),
                        &stbi_image_free);
    if (!pixels || width <= 0 || height <= 0)
      return image_decode_error::invalid_data;
    const auto width_size = static_cast<std::size_t>(width);
    const auto height_size = static_cast<std::size_t>(height);
    if (width_size > std::numeric_limits<std::size_t>::max() / height_size)
      return image_decode_error::numeric_overflow;
    const auto pixel_count = width_size * height_size;
    if (pixel_count > std::numeric_limits<std::size_t>::max() / 4)
      return image_decode_error::numeric_overflow;

    image candidate;
    candidate.rgba8_pixels.resize(pixel_count * 4);
    std::memcpy(candidate.rgba8_pixels.data(), pixels.get(), candidate.rgba8_pixels.size());
    candidate.mips.push_back({.width = static_cast<std::uint32_t>(width),
                              .height = static_cast<std::uint32_t>(height),
                              .offset = 0,
                              .size = candidate.rgba8_pixels.size()});
    output = std::move(candidate);
    return image_decode_error::none;
  } catch (const std::bad_alloc&) {
    return image_decode_error::out_of_memory;
  }
}

} // namespace granit::example::gltf
