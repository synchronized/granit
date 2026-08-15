// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t k_width = 16;
constexpr std::uint32_t k_height = 16;

[[nodiscard]] bool matches_clear_color(std::span<const std::byte> pixels,
                                       std::uint32_t bytes_per_row) {
  for (std::uint32_t y = 0; y < k_height; ++y) {
    for (std::uint32_t x = 0; x < k_width; ++x) {
      const auto offset = static_cast<std::size_t>(y) * bytes_per_row + x * 4U;
      const auto red = std::to_integer<std::uint8_t>(pixels[offset]);
      const auto green = std::to_integer<std::uint8_t>(pixels[offset + 1]);
      const auto blue = std::to_integer<std::uint8_t>(pixels[offset + 2]);
      const auto alpha = std::to_integer<std::uint8_t>(pixels[offset + 3]);
      if (red != 255U || green < 127U || green > 128U || blue != 0U || alpha != 255U)
        return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char** argv) {
  if (argc > 2) {
    std::cerr << "用法：granit_texture_readback_example [输出文件.rgba]\n";
    return 2;
  }

  granit::renderer renderer;
  auto result = renderer.initialize(
      {.application_name = "Granit Texture Readback", .enable_validation = true});
  if (granit::failed(result)) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  granit::texture texture;
  result = texture.initialize(renderer.native_handle(),
                              {.format = granit::texture_format::rgba8_unorm,
                               .usage = granit::texture_usage::color_attachment |
                                        granit::texture_usage::transfer_source,
                               .width = k_width,
                               .height = k_height});
  granit::texture_view view;
  if (granit::succeeded(result))
    result = view.initialize(renderer.native_handle(), texture.native_handle());

  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = recorder.begin();
  const granit::color_attachment_desc color{
      .view = view.native_handle(), .clear_value = {1.0F, 0.5F, 0.0F, 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, k_width, k_height}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit();
  if (granit::succeeded(result))
    result = recorder.reset();
  if (granit::failed(result)) {
    std::cerr << "离屏清屏失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  const granit::texture_write_region region{.width = k_width, .height = k_height};
  granit::texture_readback_info info;
  result = texture.query_readback(region, info);
  std::vector<std::byte> pixels(static_cast<std::size_t>(info.required_size));
  if (granit::succeeded(result))
    result = texture.read(pixels, region, info);
  if (granit::failed(result)) {
    std::cerr << "读取纹理失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  if (!matches_clear_color(pixels, info.bytes_per_row)) {
    std::cerr << "回读像素与清屏颜色不一致\n";
    return 1;
  }

  if (argc == 2) {
    const std::string_view path{argv[1]};
    std::ofstream output{argv[1], std::ios::binary};
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));
    if (!output) {
      std::cerr << "写入原始像素文件失败：" << path << '\n';
      return 1;
    }
    std::cout << "已写入 " << path << "（RGBA8，" << info.width << 'x' << info.height
              << "，每行 " << info.bytes_per_row << " 字节）\n";
  } else {
    std::cout << "纹理回读验证通过（RGBA8，" << info.width << 'x' << info.height
              << "，共 " << info.required_size << " 字节）\n";
  }
  return 0;
}
