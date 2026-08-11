// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_package.h"
#include "material/material_template_gpu.h"

#include <granit/granit.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto path = std::string{GRANIT_EXAMPLE_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0)
    return {};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

bool make_package(granit::material::material_package& package) {
  using namespace granit::material;
  material_variant_desc variant{.pass = make_feature_id("opaque"),
                                .features = {},
                                .shaders = {{.stage = package_shader_stage::vertex,
                                             .entry_point = "vertex_main",
                                             .spirv = load_shader("pbr_untextured.vert.spv")},
                                            {.stage = package_shader_stage::fragment,
                                             .entry_point = "fragment_main",
                                             .spirv = load_shader("pbr_untextured.frag.spv")}},
                                .pipeline = {}};
  variant.pipeline.primitive.cull_mode = GRANIT_CULL_MODE_BACK;
  variant.pipeline.depth.test_enabled = 1;
  variant.pipeline.depth.write_enabled = 1;
  variant.pipeline.depth.compare = GRANIT_COMPARE_OPERATION_LESS_EQUAL;
  material_package_desc desc;
  desc.variants.push_back(std::move(variant));
  return material_package::build(std::move(desc), package) == package_error::none;
}

} // namespace

int main() {
  granit::renderer renderer;
  auto result =
      renderer.initialize({.application_name = "Granit Untextured PBR", .enable_validation = true});
  granit::material::material_package package;
  if (granit::failed(result) || !make_package(package)) {
    std::cerr << "无法初始化 Renderer 或构建 PBR 材质包\n";
    return 1;
  }

  granit::material::material_template_gpu material;
  result = granit::from_native(material.initialize(renderer.native_handle(), package));
  granit_graphics_pipeline pipeline = GRANIT_NULL_HANDLE;
  if (granit::succeeded(result)) {
    result = granit::from_native(
        material.acquire_pipeline({.pass = granit::material::make_feature_id("opaque"),
                                   .variant = granit::material::make_variant_key({}),
                                   .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                                   .depth_stencil_format = GRANIT_TEXTURE_FORMAT_D32_FLOAT},
                                  pipeline));
  }

  granit_texture color_texture = GRANIT_NULL_HANDLE;
  granit_texture_view color_view = GRANIT_NULL_HANDLE;
  granit_texture depth_texture = GRANIT_NULL_HANDLE;
  granit_texture_view depth_view = GRANIT_NULL_HANDLE;
  auto create_attachment = [&](granit_texture_format format, granit_texture_usage usage,
                               granit_texture& texture, granit_texture_view& view) {
    granit_texture_desc desc = GRANIT_TEXTURE_DESC_INIT;
    desc.format = format;
    desc.usage = usage;
    desc.width = 256;
    desc.height = 256;
    return granit::from_native(
        granit_texture_create_with_default_view(renderer.native_handle(), &desc, &texture, &view));
  };
  if (granit::succeeded(result))
    result =
        create_attachment(GRANIT_TEXTURE_FORMAT_RGBA8_UNORM,
                          GRANIT_TEXTURE_USAGE_COLOR_ATTACHMENT_BIT, color_texture, color_view);
  if (granit::succeeded(result))
    result = create_attachment(GRANIT_TEXTURE_FORMAT_D32_FLOAT,
                               GRANIT_TEXTURE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_texture,
                               depth_view);

  granit::command_recorder recorder;
  if (granit::succeeded(result))
    result = recorder.initialize(renderer.native_handle());
  if (granit::succeeded(result))
    result = recorder.begin();
  if (granit::succeeded(result))
    result = recorder.bind_graphics_pipeline(pipeline);
  const granit::viewport viewport{0, 0, 256, 256, 0, 1};
  const granit::scissor scissor{0, 0, 256, 256};
  if (granit::succeeded(result))
    result = recorder.set_viewports(0, std::span{&viewport, 1});
  if (granit::succeeded(result))
    result = recorder.set_scissors(0, std::span{&scissor, 1});
  const granit::color_attachment_desc color{
      .view = color_view,
      .clear_value = {.red = 0.03F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
  const granit::depth_stencil_attachment_desc depth{.view = depth_view};
  const granit::rendering_desc rendering{.color_attachments = std::span{&color, 1},
                                         .depth_stencil_attachment = &depth,
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

  if (depth_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), depth_view));
  if (depth_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), depth_texture));
  if (color_view != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_view_destroy(renderer.native_handle(), color_view));
  if (color_texture != GRANIT_NULL_HANDLE)
    static_cast<void>(granit_texture_destroy(renderer.native_handle(), color_texture));
  if (granit::failed(result)) {
    std::cerr << "离屏 PBR 绘制失败：" << granit::result_message(result) << '\n';
    return 1;
  }
  std::cout << "无纹理 PBR 离屏绘制完成\n";
  return 0;
}
