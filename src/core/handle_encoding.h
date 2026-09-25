// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_HANDLE_ENCODING_H_
#define GRANIT_CORE_HANDLE_ENCODING_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include <granit/core/types.h>

namespace granit::detail {

/** 内部不透明句柄类型；数值编码进句柄，但不属于公共 ABI。 */
enum class handle_type : std::uint8_t {
  unknown = 0x00,

  renderer = 0x01,
  buffer = 0x02,
  texture = 0x03,
  shader = 0x04,
  pipeline = 0x05,
  swapchain = 0x06,
  fence = 0x07,
  surface = 0x08,
  texture_view = 0x09,
  sampler = 0x0a,
  command_recorder = 0x0b,
  frame = 0x0c,
  pipeline_layout = 0x0d,
  bind_group_layout = 0x0e,
  bind_group = 0x0f,
  compute_pipeline = 0x10,
  upload_batch = 0x11,
  timestamp_query_pool = 0x12,
  frame_context = 0x13,
  async_operation = 0x14,
  readback_batch = 0x15,
  pipeline_warmup_batch = 0x16,
  shader_library = 0x17,

  scene_snapshot = 0x40,
  material = 0x41,
  render_pipeline = 0x42,
  mesh = 0x43,
  canvas_draw_list = 0x44,
  debug_draw_list = 0x45,
  text_draw_list = 0x46,
  text_atlas = 0x47,
  environment_map = 0x48,

  asset_tools_shader_compiler = 0x80,
  asset_tools_shader_compilation = 0x81,
  asset_tools_shader_reflection = 0x82,
  asset_tools_material_result = 0x83,
  asset_tools_texture_result = 0x84,
  asset_tools_environment_result = 0x85,
  asset_tools_shader_library_result = 0x86,
};

inline constexpr std::uint32_t handle_maximum_generation = UINT32_C(0x00ffffff);

struct decoded_handle {
  std::uint32_t slot_index{};
  std::uint32_t generation{};
  handle_type type{handle_type::unknown};
};

[[nodiscard]] constexpr granit_handle
encode_handle(std::uint32_t slot_index, std::uint32_t generation, handle_type type) noexcept {
  return (static_cast<granit_handle>(type) << 56) | (static_cast<granit_handle>(generation) << 32) |
         (static_cast<granit_handle>(slot_index) + 1);
}

[[nodiscard]] constexpr bool decode_handle(granit_handle handle, decoded_handle& decoded) noexcept {
  constexpr granit_handle index_mask = UINT64_C(0xffffffff);
  constexpr granit_handle generation_mask = UINT64_C(0x00ffffff);
  if (handle == GRANIT_NULL_HANDLE || (handle & index_mask) == 0)
    return false;
  decoded.slot_index = static_cast<std::uint32_t>(handle & index_mask) - 1;
  decoded.generation = static_cast<std::uint32_t>((handle >> 32) & generation_mask);
  decoded.type = static_cast<handle_type>(handle >> 56);
  return decoded.generation != 0 && decoded.type != handle_type::unknown;
}

[[nodiscard]] constexpr std::uint32_t next_handle_generation(std::uint32_t generation) noexcept {
  return generation == handle_maximum_generation ? 1 : generation + 1;
}

constexpr std::array registered_handle_types{
    handle_type::renderer,
    handle_type::buffer,
    handle_type::texture,
    handle_type::shader,
    handle_type::pipeline,
    handle_type::swapchain,
    handle_type::fence,
    handle_type::surface,
    handle_type::texture_view,
    handle_type::sampler,
    handle_type::command_recorder,
    handle_type::frame,
    handle_type::pipeline_layout,
    handle_type::bind_group_layout,
    handle_type::bind_group,
    handle_type::compute_pipeline,
    handle_type::upload_batch,
    handle_type::timestamp_query_pool,
    handle_type::frame_context,
    handle_type::async_operation,
    handle_type::readback_batch,
    handle_type::pipeline_warmup_batch,
    handle_type::shader_library,
    handle_type::scene_snapshot,
    handle_type::material,
    handle_type::render_pipeline,
    handle_type::mesh,
    handle_type::canvas_draw_list,
    handle_type::debug_draw_list,
    handle_type::text_draw_list,
    handle_type::text_atlas,
    handle_type::environment_map,
    handle_type::asset_tools_shader_compiler,
    handle_type::asset_tools_shader_compilation,
    handle_type::asset_tools_shader_reflection,
    handle_type::asset_tools_material_result,
    handle_type::asset_tools_texture_result,
    handle_type::asset_tools_environment_result,
    handle_type::asset_tools_shader_library_result,
};

consteval bool registered_handle_types_are_unique() {
  for (std::size_t left = 0; left < registered_handle_types.size(); ++left) {
    for (std::size_t right = left + 1; right < registered_handle_types.size(); ++right) {
      if (registered_handle_types[left] == registered_handle_types[right])
        return false;
    }
  }
  return true;
}

static_assert(registered_handle_types_are_unique(), "内部句柄类型值必须唯一");

} // namespace granit::detail

#endif
