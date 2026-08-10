// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include <cstddef>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

std::vector<std::byte> load_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  for (std::size_t index = 0; index < bytes.size(); ++index)
    result[index] = static_cast<std::byte>(bytes[index]);
  return result;
}

} // namespace

int main() {
  granit::renderer renderer;
  auto result = renderer.initialize(
      {.application_name = "Granit Offscreen Triangle", .enable_validation = true});
  if (granit::failed(result)) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(result) << '\n';
    return 1;
  }

  const auto vertex_code = load_shader("triangle.vert.spv");
  const auto fragment_code = load_shader("triangle.frag.spv");
  if (vertex_code.empty() || fragment_code.empty()) {
    std::cerr << "无法读取示例 Shader\n";
    return 1;
  }
  granit::shader vertex;
  granit::shader fragment;
  result = vertex.initialize(renderer.native_handle(),
                             {.stage = granit::shader_stage::vertex, .code = vertex_code});
  if (granit::succeeded(result))
    result = fragment.initialize(renderer.native_handle(),
                                 {.stage = granit::shader_stage::fragment, .code = fragment_code});
  granit::pipeline_layout layout;
  if (granit::succeeded(result))
    result = layout.initialize(renderer.native_handle());
  const granit::texture_format format = granit::texture_format::rgba8_unorm;
  granit::graphics_pipeline pipeline;
  if (granit::succeeded(result)) {
    result =
        pipeline.initialize(renderer.native_handle(), {.layout = layout.native_handle(),
                                                       .vertex_shader = vertex.native_handle(),
                                                       .fragment_shader = fragment.native_handle(),
                                                       .color_formats = std::span{&format, 1},
                                                       .vertex_buffers = {},
                                                       .primitive = {}});
  }

  granit_texture_desc texture_desc = GRANIT_TEXTURE_DESC_INIT;
  texture_desc.format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM;
  texture_desc.usage = GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT;
  texture_desc.width = 256;
  texture_desc.height = 256;
  granit_texture texture = GRANIT_NULL_HANDLE;
  granit_texture_view view = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result)) {
    result = granit::from_native(granit_texture_create_with_default_view(
        renderer.native_handle(), &texture_desc, &texture, &view));
  }

  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = recorder.begin();
  if (granit::succeeded(result))
    result = recorder.bind_graphics_pipeline(pipeline.native_handle());
  const granit::viewport viewport{0, 0, 256, 256, 0, 1};
  const granit::scissor scissor{0, 0, 256, 256};
  if (granit::succeeded(result))
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (granit::succeeded(result))
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc color{
      .view = view, .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .area = {0, 0, 256, 256}};
  if (granit::succeeded(result))
    result = recorder.begin_rendering(rendering);
  if (granit::succeeded(result))
    result = recorder.draw(3);
  if (granit::succeeded(result))
    result = recorder.end_rendering();
  if (granit::succeeded(result))
    result = recorder.end();
  if (granit::succeeded(result))
    result = recorder.submit();
  if (granit::succeeded(result))
    result = recorder.reset();
  if (view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), view));
  if (texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), texture));
  if (granit::failed(result)) {
    std::cerr << "离屏三角形失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  std::cout << "离屏三角形绘制完成\n";
  return 0;
}
