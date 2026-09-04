// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_hot_reload.h"

#include <granit/renderer/renderer.hpp>

#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::uint32_t> load_shader(std::string_view name) {
  const auto path = std::string{GRANIT_SMOKE_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0) {
    return {};
  }
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

std::string load_shader_text(std::string_view name) {
  const auto path = std::string{GRANIT_SMOKE_ASSET_DIR} + "/" + std::string{name};
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

bool make_package(std::string_view pass_name, granit::material::material_package& package) {
  using namespace granit::material;
  material_package_desc desc;
  desc.variants.push_back({.pass = make_feature_id(pass_name),
                           .features = {},
                           .shaders = {{.stage = package_shader_stage::vertex,
                                        .entry_point = "main",
                                        .spirv = load_shader("triangle.vert.spv"),
                                        .wgsl = load_shader_text("triangle.vert.wgsl")},
                                       {.stage = package_shader_stage::fragment,
                                        .entry_point = "main",
                                        .spirv = load_shader("triangle.frag.spv"),
                                        .wgsl = load_shader_text("triangle.frag.wgsl")}},
                           .pipeline = {}});
  return material_package::build(std::move(desc), package) == package_error::none;
}

} // namespace

int main() {
  granit::renderer renderer;
  const auto initialized =
      renderer.initialize({.application_name = "Granit Material Hot Reload Example"});
  if (initialized.failed()) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(initialized) << '\n';
    return 1;
  }

  granit::material::material_package fallback_package;
  granit::material::material_package initial_package;
  granit::material::material_package replacement_package;
  if (!make_package("opaque", fallback_package) || !make_package("shadow", initial_package) ||
      !make_package("opaque", replacement_package)) {
    std::cerr << "无法读取示例 SPIR-V 或构建材质包\n";
    return 1;
  }

  std::shared_ptr<granit::material::material_runtime_template> fallback;
  if (granit::material::material_runtime_template::create(
          renderer.native_handle(), std::move(fallback_package), fallback) != GRANIT_SUCCESS) {
    std::cerr << "无法创建错误材质\n";
    return 1;
  }
  granit::material::material_hot_reload_slot slot{fallback};
  if (slot.reload(renderer.native_handle(), std::move(initial_package)).result != GRANIT_SUCCESS) {
    std::cerr << "无法加载初始材质\n";
    return 1;
  }

  const granit::material::material_pipeline_request request{
      .pass = granit::material::make_feature_id("opaque"),
      .variant = granit::material::make_variant_key({}),
      .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
  const auto fallback_resolution = slot.resolve_pipeline(request);
  if (fallback_resolution.result != GRANIT_SUCCESS || !fallback_resolution.used_fallback) {
    std::cerr << "错误材质回退验证失败\n";
    return 1;
  }
  std::cout << "初始材质缺少 opaque 变体，已使用错误材质\n";

  if (slot.reload(renderer.native_handle(), std::move(replacement_package)).result !=
      GRANIT_SUCCESS) {
    std::cerr << "热替换失败，继续保留旧材质\n";
    return 1;
  }
  const auto replacement = slot.resolve_pipeline(request);
  if (replacement.result != GRANIT_SUCCESS || replacement.used_fallback) {
    std::cerr << "热替换后的材质解析失败\n";
    return 1;
  }
  std::cout << "材质热替换成功，当前 generation=" << replacement.generation << '\n';
  return 0;
}
