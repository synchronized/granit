// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/loader.h"
#include "model_viewer/application_core.h"
#include "model_viewer/screenshot_comparison.h"

#include <granit/granit.hpp>
#include <granit/pipeline/render_pipeline.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::uint32_t render_size = 512;
constexpr std::size_t rgba_size = static_cast<std::size_t>(render_size) * render_size * 4;

struct options {
  granit::renderer_backend backend{granit::renderer_backend::automatic};
  std::filesystem::path asset;
  std::filesystem::path environment;
  std::filesystem::path output;
  std::filesystem::path expected;
  granit::example::model_viewer::debug_display_mode debug_display{
      granit::example::model_viewer::debug_display_mode::shaded};
  granit_sample_count sample_count{GRANIT_SAMPLE_COUNT_4};
  float sampler_anisotropy{8.0F};
  bool enable_fxaa{true};
  bool enable_specular_aa{true};
  bool validation{};
};

void print_usage() {
  std::cerr << "用法：granit_model_viewer_offscreen_acceptance --asset <文件> --output <文件.rgba> "
               "[--environment <文件.grenv>] [--expected <文件.rgba>] "
               "[--debug-display=shaded|base-color|normals|metallic|roughness|"
               "geometric-normals|sampled-normals|vertex-normals|vertex-tangents] "
               "[--backend=auto|vulkan] [--msaa=1|4] [--fxaa=on|off] "
               "[--specular-aa=on|off] [--anisotropy=1|2|4|8|16] [--validation]\n";
}

bool parse_switch(std::string_view value, bool& output) {
  if (value == "on")
    output = true;
  else if (value == "off")
    output = false;
  else
    return false;
  return true;
}

bool parse_anisotropy(std::string_view value, float& output) {
  unsigned parsed = 0;
  const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), parsed);
  if (error != std::errc{} || end != value.data() + value.size() ||
      (parsed != 1 && parsed != 2 && parsed != 4 && parsed != 8 && parsed != 16))
    return false;
  output = static_cast<float>(parsed);
  return true;
}

bool parse_debug_display(std::string_view value,
                         granit::example::model_viewer::debug_display_mode& mode) {
  using enum granit::example::model_viewer::debug_display_mode;
  if (value == "shaded")
    mode = shaded;
  else if (value == "base-color")
    mode = base_color;
  else if (value == "normals")
    mode = normals;
  else if (value == "metallic")
    mode = metallic;
  else if (value == "roughness")
    mode = roughness;
  else if (value == "geometric-normals")
    mode = geometric_normals;
  else if (value == "sampled-normals")
    mode = sampled_normals;
  else if (value == "vertex-normals")
    mode = vertex_normals;
  else if (value == "vertex-tangents")
    mode = vertex_tangents;
  else
    return false;
  return true;
}

bool parse_backend(std::string_view value, granit::renderer_backend& backend) {
  if (value == "auto")
    backend = granit::renderer_backend::automatic;
  else if (value == "vulkan")
    backend = granit::renderer_backend::vulkan;
  else
    return false;
  return true;
}

bool parse_options(int argc, char** argv, options& output) {
  options candidate;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument{argv[index]};
    constexpr std::string_view backend_prefix = "--backend=";
    constexpr std::string_view debug_display_prefix = "--debug-display=";
    constexpr std::string_view msaa_prefix = "--msaa=";
    constexpr std::string_view fxaa_prefix = "--fxaa=";
    constexpr std::string_view specular_aa_prefix = "--specular-aa=";
    constexpr std::string_view anisotropy_prefix = "--anisotropy=";
    if (argument.starts_with(backend_prefix)) {
      if (!parse_backend(argument.substr(backend_prefix.size()), candidate.backend))
        return false;
    } else if (argument.starts_with(debug_display_prefix)) {
      if (!parse_debug_display(argument.substr(debug_display_prefix.size()),
                               candidate.debug_display))
        return false;
    } else if (argument.starts_with(msaa_prefix)) {
      const auto value = argument.substr(msaa_prefix.size());
      if (value == "1")
        candidate.sample_count = GRANIT_SAMPLE_COUNT_1;
      else if (value == "4")
        candidate.sample_count = GRANIT_SAMPLE_COUNT_4;
      else
        return false;
    } else if (argument.starts_with(fxaa_prefix)) {
      if (!parse_switch(argument.substr(fxaa_prefix.size()), candidate.enable_fxaa))
        return false;
    } else if (argument.starts_with(specular_aa_prefix)) {
      if (!parse_switch(argument.substr(specular_aa_prefix.size()), candidate.enable_specular_aa))
        return false;
    } else if (argument.starts_with(anisotropy_prefix)) {
      if (!parse_anisotropy(argument.substr(anisotropy_prefix.size()),
                            candidate.sampler_anisotropy))
        return false;
    } else if (argument == "--asset" && index + 1 < argc) {
      candidate.asset = argv[++index];
    } else if (argument == "--environment" && index + 1 < argc) {
      candidate.environment = argv[++index];
    } else if (argument == "--output" && index + 1 < argc) {
      candidate.output = argv[++index];
    } else if (argument == "--expected" && index + 1 < argc) {
      candidate.expected = argv[++index];
    } else if (argument == "--validation") {
      candidate.validation = true;
    } else {
      return false;
    }
  }
  if (candidate.asset.empty() || candidate.output.empty())
    return false;
  output = std::move(candidate);
  return true;
}

bool read_file(const std::filesystem::path& path, std::vector<std::byte>& bytes) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  if (!input)
    return false;
  const auto end = input.tellg();
  if (end < 0)
    return false;
  std::vector<std::byte> candidate(static_cast<std::size_t>(end));
  input.seekg(0);
  if (!candidate.empty())
    input.read(reinterpret_cast<char*>(candidate.data()), end);
  if (!input)
    return false;
  bytes = std::move(candidate);
  return true;
}

bool write_file(const std::filesystem::path& path, std::span<const std::uint8_t> bytes) {
  std::error_code directory_error;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), directory_error);
  if (directory_error)
    return false;
  std::ofstream output(path, std::ios::binary);
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  return static_cast<bool>(output);
}

bool write_text(const std::filesystem::path& path, std::string_view text) {
  std::error_code directory_error;
  if (path.has_parent_path())
    std::filesystem::create_directories(path.parent_path(), directory_error);
  if (directory_error)
    return false;
  std::ofstream output(path);
  output << text;
  return static_cast<bool>(output);
}

std::filesystem::path sidecar_path(const std::filesystem::path& output, std::string_view suffix) {
  auto result = output;
  result.replace_extension();
  result += suffix;
  return result;
}

class file_resolver final : public granit::example::gltf::resource_resolver {
public:
  explicit file_resolver(std::filesystem::path base) : base_(std::move(base)) {}

  [[nodiscard]] bool resolve(std::string_view path, std::vector<std::byte>& bytes) const override {
    return read_file(base_ / std::filesystem::path(path), bytes);
  }

private:
  std::filesystem::path base_;
};

bool compare_expected(const options& arguments, const granit::renderer_info& renderer,
                      std::span<const std::uint8_t> actual) {
  if (arguments.expected.empty())
    return true;
  std::vector<std::byte> expected_bytes;
  if (!read_file(arguments.expected, expected_bytes) || expected_bytes.size() != rgba_size) {
    std::cerr << "期望截图不是 512x512 紧密 RGBA8 文件\n";
    return false;
  }
  const auto expected = std::span{reinterpret_cast<const std::uint8_t*>(expected_bytes.data()),
                                  expected_bytes.size()};
  // 单采样基准没有 MSAA/FXAA 的轮廓稳定性，允许极少量后端光栅化边缘差异；
  // 高质量路径继续使用严格阈值，颜色和深度阈值不变。
  const granit::example::model_viewer::screenshot_comparison_options comparison_options{
      .max_silhouette_mismatch_count = arguments.sample_count == GRANIT_SAMPLE_COUNT_1 ? 64U : 4U};
  granit::example::model_viewer::screenshot_comparison_report report;
  const auto error = granit::example::model_viewer::compare_screenshots(
      {render_size, render_size, expected, {}}, {render_size, render_size, actual, {}},
      comparison_options, report);
  if (error != granit::example::model_viewer::screenshot_comparison_error::none) {
    std::cerr << "截图比较参数无效：" << static_cast<int>(error) << '\n';
    return false;
  }
  std::cout << "截图比较：轮廓错误=" << report.silhouette_mismatch_count
            << "，颜色 MAE=" << report.color_mean_absolute_error
            << "，颜色异常比例=" << report.color_outlier_ratio << '\n';
  if (report.passed)
    return true;

  std::vector<std::uint8_t> difference(rgba_size);
  for (std::size_t pixel = 0; pixel < rgba_size / 4; ++pixel) {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      const auto offset = pixel * 4 + channel;
      const auto absolute = std::abs(static_cast<int>(expected[offset]) - actual[offset]);
      difference[offset] = static_cast<std::uint8_t>(std::min(absolute * 4, 255));
    }
    difference[pixel * 4 + 3] = 255;
  }
  const auto difference_path = sidecar_path(arguments.output, ".diff.rgba");
  const auto report_path = sidecar_path(arguments.output, ".report.json");
  const auto backend = renderer.backend == granit::renderer_backend::webgpu ? "webgpu" : "vulkan";
  std::ostringstream json;
  json << "{\n"
       << "  \"schema_version\": 1,\n"
       << "  \"passed\": false,\n"
       << "  \"backend\": " << std::quoted(backend) << ",\n"
       << "  \"adapter\": " << std::quoted(renderer.adapter_name) << ",\n"
       << "  \"asset\": " << std::quoted(arguments.asset.generic_string()) << ",\n"
       << "  \"expected\": " << std::quoted(arguments.expected.generic_string()) << ",\n"
       << "  \"actual\": " << std::quoted(arguments.output.generic_string()) << ",\n"
       << "  \"difference\": " << std::quoted(difference_path.generic_string()) << ",\n"
       << "  \"width\": " << render_size << ",\n"
       << "  \"height\": " << render_size << ",\n"
       << "  \"edge_tolerance_pixels\": "
       << static_cast<unsigned>(comparison_options.edge_tolerance_pixels) << ",\n"
       << "  \"max_silhouette_mismatch_count\": "
       << comparison_options.max_silhouette_mismatch_count << ",\n"
       << "  \"color_channel_threshold\": "
       << static_cast<unsigned>(comparison_options.color_channel_threshold) << ",\n"
       << "  \"max_color_mean_absolute_error\": "
       << comparison_options.max_color_mean_absolute_error << ",\n"
       << "  \"max_color_outlier_ratio\": " << comparison_options.max_color_outlier_ratio << ",\n"
       << "  \"silhouette_mismatch_count\": " << report.silhouette_mismatch_count << ",\n"
       << "  \"compared_color_pixel_count\": " << report.compared_color_pixel_count << ",\n"
       << "  \"color_mean_absolute_error\": " << report.color_mean_absolute_error << ",\n"
       << "  \"color_outlier_count\": " << report.color_outlier_count << ",\n"
       << "  \"color_outlier_ratio\": " << report.color_outlier_ratio << "\n"
       << "}\n";
  if (!write_file(difference_path, difference) || !write_text(report_path, json.str())) {
    std::cerr << "写入截图差异产物失败\n";
  } else {
    std::cerr << "截图差异图：" << difference_path << "\n诊断报告：" << report_path << '\n';
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  options arguments;
  if (!parse_options(argc, argv, arguments)) {
    print_usage();
    return 2;
  }

  granit::renderer renderer;
  std::string_view stage = "创建 Renderer";
  const auto diagnostics = [](granit_diagnostic_severity, granit_diagnostic_category,
                              const char* message, std::uint32_t message_length, void*) noexcept {
    if (message != nullptr && message_length != 0)
      std::cerr << "[granit] " << std::string_view{message, message_length} << '\n';
  };
  auto result = renderer.initialize({.application_name = "Granit Model Viewer Acceptance",
                                     .enable_validation = arguments.validation,
                                     .diagnostics = diagnostics,
                                     .backend = arguments.backend});
  granit::renderer_info renderer_details;
  if (result.ok()) {
    stage = "查询 Renderer 信息";
    result = renderer.get_info(renderer_details);
  }
  granit::renderer_limits renderer_limits;
  if (result.ok()) {
    stage = "查询 Renderer 限制";
    result = renderer.get_limits(renderer_limits);
  }
  if (result.ok() && ((renderer_limits.framebuffer_sample_counts & arguments.sample_count) == 0 ||
                      arguments.sampler_anisotropy > renderer_limits.max_sampler_anisotropy)) {
    stage = "校验质量配置";
    result = granit::result::unsupported;
  }
  granit::example::model_viewer::application_core core;
  if (result.ok()) {
    stage = "启动应用 Core";
    result = core.begin_renderer();
  }
  if (result.ok()) {
    stage = "确认 Renderer 就绪";
    result = core.renderer_ready();
  }

  std::vector<std::byte> asset_bytes;
  stage = "读取模型主文件";
  if (result.ok() && !read_file(arguments.asset, asset_bytes))
    result = granit::result::invalid_argument;
  file_resolver resolver(arguments.asset.parent_path());
  if (result.ok()) {
    stage = "解析模型资产";
    result = core.load_asset(asset_bytes, &resolver);
  }
  if (result.ok()) {
    stage = "上传 GPU Scene";
    std::vector<std::byte> environment_bytes;
    if (!arguments.environment.empty() && !read_file(arguments.environment, environment_bytes)) {
      stage = "读取环境包";
      result = granit::result::invalid_argument;
    } else {
      result =
          core.upload(renderer.native_handle(), environment_bytes, arguments.sampler_anisotropy);
    }
  }

  granit::texture output_texture;
  granit::texture_view output_view;
  if (result.ok()) {
    stage = "创建离屏颜色纹理";
    result = output_texture.initialize(
        renderer.native_handle(),
        {.format = granit::texture_format::rgba8_unorm,
         .usage = granit::texture_usage::color_attachment | granit::texture_usage::transfer_source,
         .width = render_size,
         .height = render_size});
  }
  if (result.ok()) {
    stage = "创建离屏颜色视图";
    result = output_view.initialize(renderer.native_handle(), output_texture.native_handle());
  }
  granit::render_pipeline pipeline;
  if (result.ok()) {
    stage = "创建 Render Pipeline";
    granit_render_pipeline_desc pipeline_desc = GRANIT_RENDER_PIPELINE_DESC_INIT;
    pipeline_desc.sample_count = arguments.sample_count;
    pipeline_desc.enable_fxaa = arguments.enable_fxaa;
    pipeline_desc.enable_specular_aa = arguments.enable_specular_aa;
    result = pipeline.initialize(renderer.native_handle(), pipeline_desc);
  }

  granit::example::model_viewer::viewer_change diagnostic_change{};
  diagnostic_change.debug_display = arguments.debug_display;
  for (std::uint32_t frame = 0; frame < 3 && result.ok(); ++frame) {
    granit::example::model_viewer::application_tick_output tick;
    stage = "更新固定相机场景";
    result = core.tick({.input = {},
                        .change = diagnostic_change,
                        .width = render_size,
                        .height = render_size,
                        .performance = {}},
                       tick);
    if (result.ok()) {
      stage = "渲染离屏帧";
      tick.render.output = output_view.native_handle();
      tick.render.output_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
      tick.render.clear_color = {0.0F, 0.0F, 0.0F, 1.0F};
      result = pipeline.render(tick.render);
    }
  }
  if (result.failed()) {
    std::cerr << stage << "失败：" << granit::result_message(result);
    if (!core.diagnostic().empty())
      std::cerr << "（" << core.diagnostic() << "）";
    std::cerr << '\n';
    return 1;
  }

  const granit::texture_write_region region{.width = render_size, .height = render_size};
  granit::texture_readback_info info;
  result = output_texture.query_readback(region, info);
  std::vector<std::byte> padded(static_cast<std::size_t>(info.required_size));
  if (result.ok())
    result = output_texture.read(padded, region, info);
  if (result.failed()) {
    std::cerr << "回读模型截图失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  std::vector<std::uint8_t> rgba(rgba_size);
  constexpr auto tight_row_size = static_cast<std::size_t>(render_size) * 4;
  for (std::uint32_t y = 0; y < render_size; ++y) {
    const auto* source = padded.data() + static_cast<std::size_t>(y) * info.bytes_per_row;
    auto* destination = rgba.data() + static_cast<std::size_t>(y) * tight_row_size;
    std::memcpy(destination, source, tight_row_size);
  }
  if (!write_file(arguments.output, rgba)) {
    std::cerr << "写入实际截图失败：" << arguments.output << '\n';
    return 1;
  }
  if (!compare_expected(arguments, renderer_details, rgba)) {
    std::cerr << "固定截图回归失败，实际图已写入：" << arguments.output << '\n';
    return 1;
  }
  std::cout << "固定截图验收通过：MSAA=" << arguments.sample_count
            << "x，FXAA=" << (arguments.enable_fxaa ? "on" : "off")
            << "，Specular AA=" << (arguments.enable_specular_aa ? "on" : "off")
            << "，Anisotropy=" << arguments.sampler_anisotropy << "x；实际图已写入："
            << arguments.output << '\n';
  return 0;
}
