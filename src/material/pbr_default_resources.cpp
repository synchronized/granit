// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/pbr_default_resources.h"

#include "material/pbr_material_schema.h"

#include <array>
#include <cstddef>

namespace granit::material {
namespace {

constexpr std::array formats{
    granit::texture_format::rgba8_srgb, granit::texture_format::rgba8_unorm,
    granit::texture_format::rgba8_unorm, granit::texture_format::rgba8_unorm,
    granit::texture_format::rgba8_srgb};
constexpr std::array<std::array<std::byte, 4>, 5> pixels{
    std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}},
    std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}},
    std::array{std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}},
    std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}},
    std::array{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{255}}};
} // namespace

granit_result pbr_default_resources::initialize(granit_renderer renderer) noexcept {
  if (renderer == GRANIT_NULL_HANDLE || initialized())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::size_t index = 0; index < textures_.size(); ++index) {
    auto result = textures_[index].initialize(
        renderer,
        {.format = formats[index],
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination});
    if (granit::failed(result)) {
      static_cast<void>(reset());
      return static_cast<granit_result>(result);
    }
    result = textures_[index].write(pixels[index], {}, {});
    if (granit::failed(result)) {
      static_cast<void>(reset());
      return static_cast<granit_result>(result);
    }
    result = views_[index].initialize(renderer, textures_[index].native_handle());
    if (granit::failed(result)) {
      static_cast<void>(reset());
      return static_cast<granit_result>(result);
    }
  }
  const auto result = sampler_.initialize(renderer);
  if (granit::failed(result)) {
    static_cast<void>(reset());
    return static_cast<granit_result>(result);
  }
  return GRANIT_SUCCESS;
}

granit_result pbr_default_resources::reset() noexcept {
  auto first = static_cast<granit_result>(sampler_.reset());
  for (auto& view : views_) {
    const auto result = static_cast<granit_result>(view.reset());
    if (first == GRANIT_SUCCESS && result != GRANIT_SUCCESS)
      first = result;
  }
  for (auto& texture : textures_) {
    const auto result = static_cast<granit_result>(texture.reset());
    if (first == GRANIT_SUCCESS && result != GRANIT_SUCCESS)
      first = result;
  }
  return first;
}

granit_result pbr_default_resources::bind(material_gpu_instance& instance) const noexcept {
  if (!initialized() || !instance.initialized())
    return GRANIT_ERROR_INVALID_ARGUMENT;
  for (std::size_t index = 0; index < views_.size(); ++index) {
    if (instance.set_resource(make_parameter_id(pbr_texture_parameter_names[index]),
                              parameter_type::texture_view,
                              views_[index].native_handle()) != metadata_error::none) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
  }
  return instance.set_resource(make_parameter_id(pbr_sampler_parameter_name),
                               parameter_type::sampler,
                               sampler_.native_handle()) == metadata_error::none
             ? GRANIT_SUCCESS
             : GRANIT_ERROR_INVALID_ARGUMENT;
}

} // namespace granit::material
