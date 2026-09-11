// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_hot_reload.h"
#include "../support/shader_asset_store.h"

#include <granit/renderer/renderer.hpp>

#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

bool make_package(const granit::tests::shader_asset_store& assets, std::string_view pass_name,
                  granit::material::material_package& package) {
  using namespace granit::material;
  material_package_desc desc;
  desc.variants.push_back({.pass = make_feature_id(pass_name),
                           .features = {},
                           .shaders = {assets.reference(std::string{GRANIT_SMOKE_ASSET_DIR} +
                                                        "/triangle.vert.grshaderobj"),
                                       assets.reference(std::string{GRANIT_SMOKE_ASSET_DIR} +
                                                        "/triangle.frag.grshaderobj")},
                           .pipeline = {}});
  return material_package::build(std::move(desc), package) == package_error::none;
}

} // namespace

int main() {
  granit::tests::shader_asset_store assets;
  if (!assets.add(std::string{GRANIT_SMOKE_ASSET_DIR} + "/triangle.vert.grshaderobj") ||
      !assets.add(std::string{GRANIT_SMOKE_ASSET_DIR} + "/triangle.frag.grshaderobj")) {
    std::cerr << "无法读取 Smoke Shader Asset\n";
    return 1;
  }
  granit::renderer renderer;
  const auto initialized =
      renderer.initialize({.application_name = "Granit Material Hot Reload Example"});
  if (initialized.failed()) {
    std::cerr << "创建 Renderer 失败：" << granit::result_message(initialized) << '\n';
    return 1;
  }
  std::vector<std::byte> shader_library_bytes;
  granit::shader_library shader_library;
  if (!assets.initialize_library(renderer.native_handle(), shader_library_bytes, shader_library)) {
    std::cerr << "无法创建 Shader Library\n";
    return 1;
  }

  granit::material::material_package fallback_package;
  granit::material::material_package initial_package;
  granit::material::material_package replacement_package;
  if (!make_package(assets, "opaque", fallback_package) ||
      !make_package(assets, "shadow", initial_package) ||
      !make_package(assets, "opaque", replacement_package)) {
    std::cerr << "无法读取 Smoke Shader Asset 或构建材质包\n";
    return 1;
  }

  std::shared_ptr<granit::material::material_runtime_template> fallback;
  if (granit::material::material_runtime_template::create(
          renderer.native_handle(), std::move(fallback_package), fallback,
          shader_library.native_handle()) != GRANIT_SUCCESS) {
    std::cerr << "无法创建错误材质\n";
    return 1;
  }
  granit::material::material_hot_reload_slot slot{fallback};
  if (slot.reload(renderer.native_handle(), std::move(initial_package),
                  shader_library.native_handle())
          .result != GRANIT_SUCCESS) {
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

  if (slot.reload(renderer.native_handle(), std::move(replacement_package),
                  shader_library.native_handle())
          .result != GRANIT_SUCCESS) {
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
