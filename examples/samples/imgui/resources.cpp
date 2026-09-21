// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "samples/imgui/resources.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <span>
#include <vector>

namespace granit::example {

result resolve_imgui_sample_texture(ImTextureID texture, granit_canvas_draw_state& state,
                                    void* user_data) noexcept {
  if (user_data == nullptr)
    return result::invalid_argument;
  const auto& bindings = *static_cast<const imgui_sample_texture_bindings*>(user_data);
  const auto* binding = texture == imgui_font_texture_id      ? &bindings.font
                        : texture == imgui_checker_texture_id ? &bindings.checker
                                                              : nullptr;
  if (binding == nullptr) {
    std::fprintf(stderr, "ImGui 绘制命令使用了未识别的纹理 ID：0x%llx\n",
                 static_cast<unsigned long long>(texture));
    return result::invalid_argument;
  }
  state.texture = binding->view;
  state.sampler = binding->sampler;
  return result::success;
}

result upload_imgui_checker_texture(renderer& renderer, texture& output, texture_view& view) {
  constexpr std::array<std::uint8_t, 16> pixels{238, 194, 255, 255, 35,  31, 52,  255,
                                                35,  31,  52,  255, 104, 87, 204, 255};
  auto upload_result = output.initialize(
      renderer, {.format = texture_format::rgba8_unorm,
                 .usage = texture_usage::sampled | texture_usage::transfer_destination,
                 .width = 2,
                 .height = 2});
  if (upload_result.ok()) {
    upload_result =
        output.write(std::as_bytes(std::span{pixels}), {.bytes_per_row = 8, .rows_per_image = 2},
                     {.width = 2, .height = 2});
  }
  if (upload_result.ok())
    upload_result = view.initialize(renderer, output);
  return upload_result;
}

result upload_imgui_font_atlas(renderer& renderer, texture& output, texture_view& view,
                               sampler& output_sampler) {
  unsigned char* pixels = nullptr;
  int width = 0;
  int height = 0;
  ImGui::GetIO().Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
  if (pixels == nullptr || width <= 0 || height <= 0)
    return result::internal;

  const auto byte_count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  std::vector<std::byte> premultiplied_pixels(byte_count);
  for (std::size_t offset = 0; offset < byte_count; offset += 4) {
    const auto alpha = pixels[offset + 3];
    for (std::size_t channel = 0; channel < 3; ++channel) {
      premultiplied_pixels[offset + channel] = static_cast<std::byte>(
          (static_cast<std::uint32_t>(pixels[offset + channel]) * alpha + 127U) / 255U);
    }
    premultiplied_pixels[offset + 3] = static_cast<std::byte>(alpha);
  }

  auto upload_result = output.initialize(
      renderer, {.format = texture_format::rgba8_unorm,
                 .usage = texture_usage::sampled | texture_usage::transfer_destination,
                 .width = static_cast<std::uint32_t>(width),
                 .height = static_cast<std::uint32_t>(height)});
  if (upload_result.ok()) {
    upload_result = output.write(
        premultiplied_pixels,
        {.bytes_per_row = static_cast<std::uint32_t>(width) * 4,
         .rows_per_image = static_cast<std::uint32_t>(height)},
        {.width = static_cast<std::uint32_t>(width), .height = static_cast<std::uint32_t>(height)});
  }
  if (upload_result.ok())
    upload_result = view.initialize(renderer, output);
  if (upload_result.ok()) {
    upload_result = output_sampler.initialize(renderer, {.address_u = address_mode::clamp_to_edge,
                                                         .address_v = address_mode::clamp_to_edge,
                                                         .address_w = address_mode::clamp_to_edge});
  }
  if (upload_result.ok()) {
    ImGui::GetIO().Fonts->SetTexID(imgui_font_texture_id);
    ImGui::GetIO().Fonts->TexRef._TexData->SetStatus(ImTextureStatus_OK);
  }
  return upload_result;
}

bool imgui_target_needs_srgb_encoding(texture_format format) noexcept {
  return format == texture_format::rgba8_unorm || format == texture_format::bgra8_unorm;
}

result record_imgui_sample_canvas(command_recorder& recorder, canvas_draw_list& canvas,
                                  texture_view_ref target, const swapchain_info& info,
                                  std::uint32_t frame_slot) noexcept {
  granit_canvas_draw_list_stats stats = GRANIT_CANVAS_DRAW_LIST_STATS_INIT;
  auto record_result = canvas.get_stats(stats);
  if (record_result.failed())
    return record_result;
  if (stats.item_count == 0) {
    const color_attachment_desc color{.view = target, .resolve_view = {}};
    const rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                   .area = {0, 0, info.width, info.height}};
    record_result = recorder.begin_rendering(rendering);
    if (record_result.ok())
      record_result = recorder.end_rendering();
    return record_result;
  }

  granit_canvas_record_desc record = GRANIT_CANVAS_RECORD_DESC_INIT;
  record.color = target.native_handle();
  record.color_format = static_cast<granit_texture_format>(info.format);
  record.width = info.width;
  record.height = info.height;
  record.load_operation = GRANIT_ATTACHMENT_LOAD_OPERATION_CLEAR;
  record.encode_srgb = imgui_target_needs_srgb_encoding(info.format) ? 1U : 0U;
  record.frame_slot = frame_slot;
  return canvas.record(recorder.native_handle(), record);
}

} // namespace granit::example
