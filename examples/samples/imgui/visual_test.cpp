// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "samples/imgui/content.h"
#include "samples/imgui/resources.h"
#include "validation/screenshot_comparison.h"
#include <array>
#include <catch2/catch_all.hpp>
#include <fstream>
#include <granit/granit.hpp>
#include <vector>

namespace {
struct imgui_context {
  imgui_context() { ImGui::CreateContext(); }
  ~imgui_context() { ImGui::DestroyContext(); }
};
}

TEST_CASE("ImGui 固定画面在 Vulkan 的字体纹理裁剪与 DPI 验收", "[imgui][visual]") {
  const auto scale = GENERATE(1U, 2U);
  CAPTURE(scale);
  imgui_context context;
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {320, 240};
  io.DisplayFramebufferScale = {static_cast<float>(scale), static_cast<float>(scale)};
  io.DeltaTime = 1.0F / 60.0F;
  granit::renderer renderer;
  REQUIRE(renderer.initialize({.application_name = "ImGui visual"}) == granit::result::success);
  granit::texture font, checker, output;
  granit::texture_view font_view, checker_view, output_view;
  granit::sampler sampler;
  REQUIRE(granit::example::upload_imgui_font_atlas(renderer, font, font_view, sampler) ==
          granit::result::success);
  REQUIRE(granit::example::upload_imgui_checker_texture(renderer, checker, checker_view) ==
          granit::result::success);
  const auto width = 320U * scale;
  const auto height = 240U * scale;
  REQUIRE(output.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                       .usage = granit::texture_usage::color_attachment |
                                                granit::texture_usage::transfer_source,
                                       .width = width,
                                       .height = height}) == granit::result::success);
  REQUIRE(output_view.initialize(renderer, output) == granit::result::success);
  granit::canvas_draw_list canvas;
  granit_canvas_draw_list_desc desc = GRANIT_CANVAS_DRAW_LIST_DESC_INIT;
  REQUIRE(canvas.initialize(renderer, desc) == granit::result::success);
  granit::example::imgui_sample_texture_bindings bindings{
      .font = {font_view.native_handle(), sampler.native_handle()},
      .checker = {checker_view.native_handle(), sampler.native_handle()}};
  granit::example::imgui_sample_state state;
  // 先建立窗口，再通过真实 ImGui 输入队列点击，检查内容状态与输出同步变化。
  auto frame = [&] {
    ImGui::NewFrame();
    granit::example::build_imgui_validation_scene(state, granit::example::imgui_checker_texture_id);
    ImGui::Render();
  };
  frame();
  frame();
  for (const bool enabled : {true, false}) {
    if (!enabled) {
      io.AddMousePosEvent(40, 176);
      io.AddMouseButtonEvent(0, true);
      frame();
      io.AddMouseButtonEvent(0, false);
      frame();
    }
    REQUIRE(state.validation_overlay == enabled);
    REQUIRE(canvas.clear() == granit::result::success);
    REQUIRE(granit::integration::imgui::append_draw_data(
                ImGui::GetDrawData(), canvas, granit::example::resolve_imgui_sample_texture,
                &bindings) == granit::result::success);
    granit::command_recorder recorder;
    REQUIRE(recorder.initialize(renderer) == granit::result::success);
    REQUIRE(recorder.begin() == granit::result::success);
    const granit::swapchain_info info{
        .width = width, .height = height, .format = granit::texture_format::rgba8_unorm};
    REQUIRE(granit::example::record_imgui_sample_canvas(
                recorder, canvas, output_view.native_handle(), info, 0) == granit::result::success);
    REQUIRE(recorder.end() == granit::result::success);
    REQUIRE(recorder.submit() == granit::result::success);
    granit::texture_readback_info read;
    const granit::texture_write_region region{.width = width, .height = height};
    REQUIRE(output.query_readback(region, read) == granit::result::success);
    std::vector<std::byte> bytes(static_cast<std::size_t>(read.required_size));
    REQUIRE(output.read(bytes, region, read) == granit::result::success);
    auto pixel = [&](unsigned x, unsigned y, unsigned channel) {
      return std::to_integer<unsigned char>(
          bytes[(y * scale) * read.bytes_per_row + (x * scale) * 4 + channel]);
    };
    // 对内部区域比较，允许通道误差；不依赖未经审阅的整幅截图基准。
    auto patch = [&](unsigned x, unsigned y, std::array<std::uint8_t, 4> color) {
      std::vector<std::uint8_t> expected(8 * 8 * 4), actual(expected.size());
      for (unsigned row = 0; row < 8; ++row)
        for (unsigned col = 0; col < 8; ++col)
          for (unsigned channel = 0; channel < 4; ++channel) {
            const auto index = (row * 8 + col) * 4 + channel;
            expected[index] = color[channel];
            actual[index] = pixel(x + col, y + row, channel);
          }
      granit::example::validation::screenshot_comparison_options options;
      options.background = {127, 127, 127, 255};
      granit::example::validation::screenshot_comparison_report report;
      REQUIRE(granit::example::validation::compare_screenshots(
                  {8, 8, expected, {}}, {8, 8, actual, {}}, options, report) ==
              granit::example::validation::screenshot_comparison_error::none);
      CAPTURE(x, y, report.color_mean_absolute_error);
      REQUIRE(report.passed);
    };
    patch(156, 86, {255, 0, 0, 255});
    patch(124, 86, {0, 0, 0, 255});
    patch(212, 86, {0, 0, 0, 255});
    patch(156, 48, {0, 0, 0, 255});
    patch(156, 136, {0, 0, 0, 255});
    patch(36, 172,
          enabled ? std::array<std::uint8_t, 4>{0, 255, 0, 255}
                  : std::array<std::uint8_t, 4>{255, 0, 0, 255});
    REQUIRE(static_cast<int>(pixel(36, 76, 0)) - pixel(76, 76, 0) > 80);
    REQUIRE(static_cast<int>(pixel(36, 76, 2)) - pixel(76, 76, 2) > 60);
    unsigned glyph_pixels = 0;
    for (unsigned y = 24; y < 44; ++y)
      for (unsigned x = 24; x < 210; ++x)
        if (pixel(x, y, 0) > 100)
          ++glyph_pixels;
    REQUIRE(glyph_pixels > 100);
    REQUIRE(glyph_pixels < 2000);
    std::ofstream image("imgui-vulkan-" + std::to_string(scale) +
                            (enabled ? "x-before.ppm" : "x-after.ppm"),
                        std::ios::binary);
    image << "P6\n" << width << ' ' << height << "\n255\n";
    for (unsigned y = 0; y < height; ++y)
      for (unsigned x = 0; x < width; ++x)
        image.write(reinterpret_cast<const char*>(bytes.data() + y * read.bytes_per_row + x * 4),
                    3);
    REQUIRE(image.good());
  }
}
