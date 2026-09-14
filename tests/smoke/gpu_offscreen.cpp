// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "../support/shader_asset_store.h"

#include <granit/granit.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t k_width = 64;
constexpr std::uint32_t k_height = 64;
constexpr std::uint8_t k_clear_red = 8;
constexpr std::uint8_t k_clear_green = 8;
constexpr std::uint8_t k_clear_blue = 13;

struct rgba8 {
  std::uint8_t red{};
  std::uint8_t green{};
  std::uint8_t blue{};
  std::uint8_t alpha{};
};

[[nodiscard]] rgba8 pixel_at(std::span<const std::byte> pixels, std::uint32_t bytes_per_row,
                             std::uint32_t x, std::uint32_t y) {
  const auto offset = static_cast<std::size_t>(y) * bytes_per_row + x * 4U;
  return {.red = std::to_integer<std::uint8_t>(pixels[offset]),
          .green = std::to_integer<std::uint8_t>(pixels[offset + 1]),
          .blue = std::to_integer<std::uint8_t>(pixels[offset + 2]),
          .alpha = std::to_integer<std::uint8_t>(pixels[offset + 3])};
}

[[nodiscard]] bool near(std::uint8_t actual, std::uint8_t expected, std::uint8_t tolerance) {
  const auto difference = std::max(actual, expected) - std::min(actual, expected);
  return difference <= tolerance;
}

[[nodiscard]] bool validate_pixels(std::span<const std::byte> pixels,
                                   const granit::texture_readback_info& info) {
  if (info.format != granit::texture_format::rgba8_unorm || info.width != k_width ||
      info.height != k_height || info.bytes_per_row < k_width * 4U ||
      info.required_size < static_cast<std::uint64_t>(info.bytes_per_row) * k_height) {
    std::cerr << "回读布局与 RGBA8 离屏目标不一致\n";
    return false;
  }

  const auto outside = pixel_at(pixels, info.bytes_per_row, 2, 2);
  if (!near(outside.red, k_clear_red, 1) || !near(outside.green, k_clear_green, 1) ||
      !near(outside.blue, k_clear_blue, 1) || outside.alpha != 255U) {
    std::cerr << "三角形外像素与清屏颜色不一致\n";
    return false;
  }

  // 中心点的理论插值色约为 (143, 102, 89)。容差覆盖像素中心与 UNORM 量化差异。
  const auto inside = pixel_at(pixels, info.bytes_per_row, k_width / 2U, k_height / 2U);
  if (!near(inside.red, 143, 12) || !near(inside.green, 102, 12) || !near(inside.blue, 89, 12) ||
      inside.alpha != 255U) {
    std::cerr << "三角形内像素与预期插值颜色不一致\n";
    return false;
  }
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "用法：granit_gpu_offscreen_smoke [输出文件.rgba]\n";
    return 2;
  }

  granit::renderer renderer;
  auto result = renderer.initialize(
      {.application_name = "Granit GPU Offscreen Smoke", .enable_validation = true});
  if (result.failed()) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  granit::shader vertex;
  granit::shader fragment;
  const auto vertex_path = std::string{GRANIT_SMOKE_ASSET_DIR} + "/triangle.vert.grshaderobj";
  const auto fragment_path = std::string{GRANIT_SMOKE_ASSET_DIR} + "/triangle.frag.grshaderobj";
  granit::tests::shader_asset_store shader_assets;
  if (!shader_assets.add(vertex_path) || !shader_assets.add(fragment_path))
    result = granit::result::invalid_argument;
  std::vector<std::byte> shader_library_bytes;
  granit::shader_library shader_library;
  if (result.ok() && !shader_assets.initialize_library(renderer.native_handle(),
                                                       shader_library_bytes, shader_library)) {
    result = granit::result::invalid_argument;
  }
  const auto vertex_reference = shader_assets.reference(vertex_path);
  const auto fragment_reference = shader_assets.reference(fragment_path);
  if (result.ok())
    result = shader_library.create_shader(vertex_reference.asset_id, vertex);
  if (result.ok())
    result = shader_library.create_shader(fragment_reference.asset_id, fragment);

  granit::pipeline_layout layout;
  if (result.ok())
    result = layout.initialize(renderer.native_handle());
  constexpr granit::texture_format format = granit::texture_format::rgba8_unorm;
  granit::graphics_pipeline pipeline;
  if (result.ok()) {
    result = pipeline.initialize(renderer.native_handle(),
                                 {
                                     .layout = layout.native_handle(),
                                     .vertex_shader = vertex.native_handle(),
                                     .fragment_shader = fragment.native_handle(),
                                     .color_formats = std::span{&format, 1},
                                 });
  }

  granit::texture texture;
  if (result.ok()) {
    result = texture.initialize(
        renderer.native_handle(),
        {.format = format,
         .usage = granit::texture_usage::color_attachment | granit::texture_usage::transfer_source,
         .width = k_width,
         .height = k_height});
  }
  granit::texture_view view;
  if (result.ok())
    result = view.initialize(renderer.native_handle(), texture.native_handle());

  granit::command_recorder recorder;
  if (result.ok())
    result = recorder.initialize(renderer.native_handle());
  if (result.ok())
    result = recorder.begin();
  if (result.ok())
    result = recorder.bind_graphics_pipeline(pipeline.native_handle());
  constexpr granit::viewport viewport{0, 0, k_width, k_height, 0, 1};
  constexpr granit::scissor scissor{0, 0, k_width, k_height};
  if (result.ok())
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (result.ok())
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc color{
      .view = view.native_handle(),
      .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, k_width, k_height}};
  if (result.ok())
    result = recorder.begin_rendering(rendering);
  if (result.ok())
    result = recorder.draw(3);
  if (result.ok())
    result = recorder.end_rendering();
  if (result.ok())
    result = recorder.end();
  if (result.ok())
    result = recorder.submit();
  if (result.ok())
    result = recorder.reset();
  if (result.failed()) {
    std::cerr << "离屏绘制失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  const granit::texture_write_region region{.width = k_width, .height = k_height};
  granit::texture_readback_info info;
  result = texture.query_readback(region, info);
  std::vector<std::byte> pixels;
  if (result.ok())
    pixels.resize(static_cast<std::size_t>(info.required_size));
  if (result.ok())
    result = texture.read(pixels, region, info);
  if (result.failed()) {
    std::cerr << "读取离屏纹理失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  if (!validate_pixels(pixels, info))
    return 1;

  if (argc == 2) {
    const std::string_view path{argv[1]};
    std::ofstream output{argv[1], std::ios::binary};
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!output) {
      std::cerr << "写入原始像素文件失败：" << path << '\n';
      return 1;
    }
    std::cout << "已写入 " << path << "（RGBA8，" << info.width << 'x' << info.height << "，每行 "
              << info.bytes_per_row << " 字节）\n";
  } else {
    std::cout << "离屏 GPU Smoke 通过\n";
  }
  return 0;
}
