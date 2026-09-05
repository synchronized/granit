// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/pipeline/environment_map.h>

#include "pipeline/environment_asset.h"

#include <granit/renderer/texture.hpp>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace {

constexpr uint64_t index_mask = UINT64_C(0xffffffff);
constexpr uint64_t generation_mask = UINT64_C(0x00ffffff);
constexpr uint64_t type_value = UINT64_C(0x45);
constexpr std::uint32_t rgba16_bytes_per_pixel = 8;

struct environment_state {
  granit_renderer renderer{GRANIT_NULL_HANDLE};
  granit::texture irradiance_texture;
  granit::texture_view irradiance_view;
  granit::texture prefiltered_texture;
  granit::texture_view prefiltered_view;
  granit::texture brdf_texture;
  granit::texture_view brdf_view;
  granit_render_pipeline_environment environment = GRANIT_RENDER_PIPELINE_ENVIRONMENT_INIT;
  float recommended_exposure_ev{};
};

struct environment_slot {
  std::shared_ptr<environment_state> state;
  uint32_t generation{1};
};

std::mutex registry_mutex;
std::vector<environment_slot> registry;

granit_handle encode(std::size_t index, uint32_t generation) noexcept {
  return (type_value << 56) | (static_cast<uint64_t>(generation) << 32) |
         (static_cast<uint64_t>(index) + 1);
}

bool decode(granit_handle handle, std::size_t& index, uint32_t& generation) noexcept {
  if ((handle >> 56) != type_value || (handle & index_mask) == 0)
    return false;
  index = static_cast<std::size_t>((handle & index_mask) - 1);
  generation = static_cast<uint32_t>((handle >> 32) & generation_mask);
  return generation != 0;
}

std::shared_ptr<environment_state> find_environment(granit_renderer renderer,
                                                    granit_environment_map handle) {
  std::size_t index{};
  uint32_t generation{};
  if (!decode(handle, index, generation))
    return {};
  std::scoped_lock lock{registry_mutex};
  if (index >= registry.size() || registry[index].generation != generation ||
      registry[index].state == nullptr || registry[index].state->renderer != renderer) {
    return {};
  }
  return registry[index].state;
}

granit::result upload_cube_mip(granit::texture& texture, std::span<const std::byte> pixels,
                               std::uint32_t resolution, std::uint32_t mip) noexcept {
  return texture.write(
      pixels, {.bytes_per_row = resolution * rgba16_bytes_per_pixel, .rows_per_image = resolution},
      {.mip_level = mip, .array_layer_count = 6, .width = resolution, .height = resolution});
}

granit_result initialize_state(environment_state& state,
                               const granit::pipeline::detail::environment_package& package) {
  auto result = state.irradiance_texture.initialize(
      state.renderer,
      {.dimension = granit::texture_dimension::cube,
       .format = granit::texture_format::rgba16_float,
       .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
       .width = package.irradiance_resolution,
       .height = package.irradiance_resolution,
       .array_layers = 6});
  if (result.ok())
    result = upload_cube_mip(state.irradiance_texture, package.irradiance_pixels,
                             package.irradiance_resolution, 0);
  if (result.ok()) {
    result = state.irradiance_view.initialize(
        state.renderer, state.irradiance_texture.native_handle(),
        {.dimension = granit::texture_dimension::cube, .array_layer_count = 6});
  }
  if (result.ok()) {
    result = state.prefiltered_texture.initialize(
        state.renderer,
        {.dimension = granit::texture_dimension::cube,
         .format = granit::texture_format::rgba16_float,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = package.prefiltered_mips.front().resolution,
         .height = package.prefiltered_mips.front().resolution,
         .mip_levels = static_cast<std::uint32_t>(package.prefiltered_mips.size()),
         .array_layers = 6});
  }
  for (std::size_t mip = 0; result.ok() && mip < package.prefiltered_mips.size(); ++mip) {
    result =
        upload_cube_mip(state.prefiltered_texture, package.prefiltered_mips[mip].pixels,
                        package.prefiltered_mips[mip].resolution, static_cast<std::uint32_t>(mip));
  }
  if (result.ok()) {
    result = state.prefiltered_view.initialize(
        state.renderer, state.prefiltered_texture.native_handle(),
        {.dimension = granit::texture_dimension::cube,
         .mip_level_count = static_cast<std::uint32_t>(package.prefiltered_mips.size()),
         .array_layer_count = 6});
  }
  if (result.ok()) {
    result = state.brdf_texture.initialize(
        state.renderer,
        {.format = granit::texture_format::rgba16_float,
         .usage = granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
         .width = package.brdf_width,
         .height = package.brdf_height});
  }
  if (result.ok()) {
    result = state.brdf_texture.write(
        package.brdf_pixels, {.bytes_per_row = package.brdf_width * rgba16_bytes_per_pixel},
        {.width = package.brdf_width, .height = package.brdf_height});
  }
  if (result.ok())
    result = state.brdf_view.initialize(state.renderer, state.brdf_texture.native_handle());
  if (result.failed())
    return static_cast<granit_result>(result);

  state.environment.irradiance = state.irradiance_view.native_handle();
  state.environment.prefiltered_environment = state.prefiltered_view.native_handle();
  state.environment.brdf_lut = state.brdf_view.native_handle();
  state.environment.intensity = package.recommended_environment_intensity;
  state.environment.prefiltered_max_mip = static_cast<float>(package.prefiltered_mips.size() - 1U);
  state.recommended_exposure_ev = package.recommended_exposure_ev;
  return GRANIT_SUCCESS;
}

granit_result register_environment(std::shared_ptr<environment_state> state,
                                   granit_environment_map& output) {
  std::scoped_lock lock{registry_mutex};
  std::size_t index{};
  while (index < registry.size() && registry[index].state != nullptr)
    ++index;
  if (index == registry.size())
    registry.emplace_back();
  registry[index].state = std::move(state);
  output = encode(index, registry[index].generation);
  return GRANIT_SUCCESS;
}

} // namespace

extern "C" granit_result
granit_environment_map_create_from_asset(granit_renderer renderer,
                                         const granit_environment_map_asset_desc* desc,
                                         granit_environment_map* environment_map) {
  if (environment_map == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *environment_map = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  if (desc == nullptr || desc->struct_size < GRANIT_ENVIRONMENT_MAP_ASSET_DESC_VERSION_1_SIZE ||
      desc->reserved != 0 || desc->data == nullptr || desc->size == 0 ||
      desc->size > std::numeric_limits<std::size_t>::max()) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  try {
    granit::pipeline::detail::environment_package package;
    const auto bytes =
        std::span{static_cast<const std::byte*>(desc->data), static_cast<std::size_t>(desc->size)};
    const auto parsed = granit::pipeline::detail::parse_environment_package(bytes, package);
    if (parsed == granit::pipeline::detail::environment_package_error::unsupported_version)
      return GRANIT_ERROR_UNSUPPORTED;
    if (parsed != granit::pipeline::detail::environment_package_error::none)
      return GRANIT_ERROR_INVALID_ARGUMENT;
    auto state = std::make_shared<environment_state>();
    state->renderer = renderer;
    const auto result = initialize_state(*state, package);
    if (result != GRANIT_SUCCESS)
      return result;
    return register_environment(std::move(state), *environment_map);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_environment_map_create_builtin(granit_renderer renderer,
                                      granit_environment_map* environment_map) {
  if (environment_map == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *environment_map = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    constexpr std::array<std::uint16_t, 24> irradiance{
        0x3266, 0x319a, 0x30cd, 0x3c00, 0x2e66, 0x2f5c, 0x30cd, 0x3c00,
        0x359a, 0x3571, 0x351f, 0x3c00, 0x291f, 0x291f, 0x2a66, 0x3c00,
        0x2fae, 0x307b, 0x311f, 0x3c00, 0x2e66, 0x2d1f, 0x2c7b, 0x3c00};
    constexpr std::array<std::uint16_t, 24> prefiltered{
        0x38cd, 0x3866, 0x3800, 0x3c00, 0x3400, 0x34cd, 0x35ae, 0x3c00,
        0x399a, 0x3971, 0x391f, 0x3c00, 0x2e66, 0x2e66, 0x2fae, 0x3c00,
        0x35ae, 0x3666, 0x3733, 0x3c00, 0x3400, 0x3266, 0x319a, 0x3c00};
    constexpr std::array<std::uint16_t, 4> brdf_lut{0x3800, 0x2e66, 0x0000, 0x3c00};
    const granit::pipeline::detail::environment_mip mip{1, std::as_bytes(std::span{prefiltered})};
    granit::pipeline::detail::environment_package package;
    package.irradiance_resolution = 1;
    package.irradiance_pixels = std::as_bytes(std::span{irradiance});
    package.prefiltered_mips = {mip};
    package.brdf_width = 1;
    package.brdf_height = 1;
    package.brdf_pixels = std::as_bytes(std::span{brdf_lut});
    auto state = std::make_shared<environment_state>();
    state->renderer = renderer;
    const auto result = initialize_state(*state, package);
    if (result != GRANIT_SUCCESS)
      return result;
    return register_environment(std::move(state), *environment_map);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_environment_map_get_info(granit_renderer renderer,
                                                         granit_environment_map environment_map,
                                                         granit_environment_map_info* info) {
  if (info == nullptr || info->struct_size < GRANIT_ENVIRONMENT_MAP_INFO_VERSION_1_SIZE ||
      info->reserved != 0 || info->reserved_tail != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  auto state = find_environment(renderer, environment_map);
  if (state == nullptr)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto size = info->struct_size;
  *info = GRANIT_ENVIRONMENT_MAP_INFO_INIT;
  info->struct_size = size;
  info->environment = state->environment;
  info->recommended_exposure_ev = state->recommended_exposure_ev;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_environment_map_destroy(granit_renderer renderer,
                                                        granit_environment_map environment_map) {
  std::size_t index{};
  uint32_t generation{};
  if (!decode(environment_map, index, generation))
    return GRANIT_ERROR_INVALID_HANDLE;
  std::shared_ptr<environment_state> removed;
  {
    std::scoped_lock lock{registry_mutex};
    if (index >= registry.size() || registry[index].generation != generation ||
        registry[index].state == nullptr || registry[index].state->renderer != renderer) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    removed = std::move(registry[index].state);
    registry[index].generation =
        registry[index].generation == generation_mask ? 1 : registry[index].generation + 1;
  }
  return GRANIT_SUCCESS;
}
