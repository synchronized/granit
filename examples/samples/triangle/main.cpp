// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <array>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string_view>
#include <vector>

#include "triangle_shader_ids.inc"

#ifndef GRANIT_TRIANGLE_SHADER_LIBRARY
#error "GRANIT_TRIANGLE_SHADER_LIBRARY must point to the generated Shader Library"
#endif

namespace {

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " 失败: " << result.message() << "\n";
  return 1;
}

std::vector<std::byte> read_file(const char* path) {
  std::ifstream stream{path, std::ios::binary};
  if (!stream)
    return {};
  stream.seekg(0, std::ios::end);
  const auto size = stream.tellg();
  if (size <= 0)
    return {};
  stream.seekg(0, std::ios::beg);
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.read(reinterpret_cast<char*>(bytes.data()), size);
  return stream ? bytes : std::vector<std::byte>{};
}

} // namespace

int main() {
  granit::renderer renderer;
  auto result = renderer.initialize({.application_name = "Granit Triangle"});
  if (result.failed())
    return report_failure("创建 Renderer", result);

  auto archive = read_file(GRANIT_TRIANGLE_SHADER_LIBRARY);
  granit::shader_library library;
  result = library.initialize(renderer.native_handle(), archive);
  if (result.failed())
    return report_failure("创建 Shader Library", result);

  granit::shader vertex_shader;
  result = library.create_shader(triangle_vertex_id, vertex_shader);
  if (result.failed())
    return report_failure("创建顶点 Shader", result);

  granit::shader fragment_shader;
  result = library.create_shader(triangle_fragment_id, fragment_shader);
  if (result.failed())
    return report_failure("创建片段 Shader", result);

  granit::pipeline_layout layout;
  result = layout.initialize(renderer.native_handle());
  if (result.failed())
    return report_failure("创建 Pipeline Layout", result);

  constexpr auto format = granit::texture_format::rgba8_unorm;
  granit::graphics_pipeline pipeline;
  result = pipeline.initialize(renderer.native_handle(),
                               {.layout = layout.native_handle(),
                                .vertex_shader = vertex_shader.native_handle(),
                                .fragment_shader = fragment_shader.native_handle(),
                                .color_formats = std::span{&format, 1}});
  if (result.failed())
    return report_failure("创建 Graphics Pipeline", result);

  granit::texture output;
  result = output.initialize(renderer.native_handle(),
                             {.format = format,
                              .usage = granit::texture_usage::color_attachment |
                                       granit::texture_usage::transfer_source,
                              .width = 64,
                              .height = 64});
  if (result.failed())
    return report_failure("创建离屏纹理", result);

  granit::texture_view output_view;
  result = output_view.initialize(renderer.native_handle(), output.native_handle());
  if (result.failed())
    return report_failure("创建离屏纹理视图", result);

  granit::command_recorder recorder;
  result = recorder.initialize(renderer.native_handle());
  if (result.failed())
    return report_failure("创建 Command Recorder", result);
  result = recorder.begin();
  if (result.failed())
    return report_failure("开始录制命令", result);

  const granit::viewport viewport{0, 0, 64, 64, 0, 1};
  const granit::scissor scissor{0, 0, 64, 64};
  const granit::color_attachment_desc color{.view = output_view.native_handle(),
                                            .clear_value = {.red = 0.05F,
                                                            .green = 0.05F,
                                                            .blue = 0.05F,
                                                            .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 64, 64}};
  if ((result = recorder.set_viewports(0, std::span{&viewport, 1})).failed() ||
      (result = recorder.set_scissors(0, std::span{&scissor, 1})).failed() ||
      (result = recorder.begin_rendering(rendering)).failed() ||
      (result = recorder.bind_graphics_pipeline(pipeline.native_handle())).failed() ||
      (result = recorder.draw(3)).failed() ||
      (result = recorder.end_rendering()).failed() ||
      (result = recorder.end()).failed() ||
      (result = recorder.submit()).failed()) {
    return report_failure("录制或提交三角形", result);
  }

  for (int attempt = 0; attempt < 8; ++attempt) {
    if ((result = renderer.process_events()).failed())
      return report_failure("推进 Renderer 事件", result);
  }

  std::array<std::byte, 4> pixel{};
  granit::texture_readback_info readback;
  result = output.read(pixel, {.x = 32, .y = 32, .width = 1, .height = 1}, readback);
  if (result.failed())
    return report_failure("读取三角形中心像素", result);

  std::cout << "Triangle rendered, center pixel: "
            << std::to_integer<unsigned int>(pixel[0]) << ','
            << std::to_integer<unsigned int>(pixel[1]) << ','
            << std::to_integer<unsigned int>(pixel[2]) << '\n';
  return 0;
}
