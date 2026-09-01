// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_hot_reload.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

namespace {

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

std::vector<std::uint32_t> load_shader(const char* name) {
  std::ifstream stream{std::string{GRANIT_TEST_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  if (!words.empty()) {
    std::memcpy(words.data(), bytes.data(), words.size() * sizeof(std::uint32_t));
  }
  return words;
}

std::string load_text(const char* name) {
  std::ifstream stream{std::string{GRANIT_TEST_ASSET_DIR} + "/" + name, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

granit::material::material_package build_package(std::string parameter_name,
                                                 std::string pass_name = "opaque") {
  using namespace granit::material;
  material_package_desc desc;
  desc.metadata.constant_buffer_size = 4;
  desc.metadata.parameters = {{.name = std::move(parameter_name),
                               .type = parameter_type::float32,
                               .offset = 0,
                               .default_value = {}}};
  desc.variants.push_back({.pass = make_feature_id(pass_name),
                           .features = {},
                           .shaders = {{.stage = package_shader_stage::vertex,
                                        .entry_point = "main",
                                        .spirv = load_shader("minimal.vert.spv"),
                                        .wgsl = load_text("dynamic_uniform.vert.wgsl")},
                                       {.stage = package_shader_stage::fragment,
                                        .entry_point = "main",
                                        .spirv = load_shader("minimal.frag.spv"),
                                        .wgsl = load_text("dynamic_uniform.frag.wgsl")}},
                           .pipeline = {}});
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  return package;
}

} // namespace

TEST_CASE("材质热替换成功发布新模板且快照保留旧模板生命周期") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-material-reload"});
  if (environment_unavailable(initialized)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  granit::material::material_hot_reload_slot slot;
  const auto first = slot.reload(renderer.native_handle(), build_package("first"));
  REQUIRE(first.result == GRANIT_SUCCESS);
  REQUIRE(first.outcome == granit::material::material_reload_outcome::replaced);
  auto old_snapshot = slot.snapshot();
  REQUIRE(old_snapshot != nullptr);

  const auto second = slot.reload(renderer.native_handle(), build_package("second"));
  REQUIRE(second.result == GRANIT_SUCCESS);
  CHECK(second.generation == first.generation + 1);
  const auto current = slot.snapshot();
  REQUIRE(current != nullptr);
  CHECK(current != old_snapshot);
  CHECK(old_snapshot->package().metadata().find(granit::material::make_parameter_id("first")) !=
        nullptr);

  const auto failed = slot.reload(GRANIT_NULL_HANDLE, build_package("invalid"));
  CHECK(failed.result == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(failed.outcome == granit::material::material_reload_outcome::retained_previous);
  CHECK(failed.generation == second.generation);
  CHECK(slot.snapshot() == current);
}

TEST_CASE("材质首次加载失败时使用调用方提供的错误材质") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-material-fallback"});
  if (environment_unavailable(initialized)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  std::shared_ptr<granit::material::material_runtime_template> fallback;
  REQUIRE(granit::material::material_runtime_template::create(
              renderer.native_handle(), build_package("error_color"), fallback) == GRANIT_SUCCESS);
  granit::material::material_hot_reload_slot slot{fallback};
  const auto failed = slot.reload(GRANIT_NULL_HANDLE, build_package("invalid"));
  CHECK(failed.result == GRANIT_ERROR_INVALID_ARGUMENT);
  CHECK(failed.outcome == granit::material::material_reload_outcome::using_fallback);
  CHECK(failed.generation == 0);
  CHECK(slot.snapshot() == fallback);
}

TEST_CASE("活动材质缺少变体时回退到错误材质 Pipeline") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-material-error"});
  if (environment_unavailable(initialized)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  std::shared_ptr<granit::material::material_runtime_template> fallback;
  REQUIRE(granit::material::material_runtime_template::create(
              renderer.native_handle(), build_package("error_color"), fallback) == GRANIT_SUCCESS);
  granit::material::material_hot_reload_slot slot{fallback};
  REQUIRE(slot.reload(renderer.native_handle(), build_package("color", "shadow")).result ==
          GRANIT_SUCCESS);

  const granit::material::material_pipeline_request request{
      .pass = granit::material::make_feature_id("opaque"),
      .variant = granit::material::make_variant_key({}),
      .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
  const auto resolved = slot.resolve_pipeline(request);
  REQUIRE(resolved.result == GRANIT_SUCCESS);
  CHECK(resolved.primary_result == GRANIT_ERROR_NOT_READY);
  CHECK(resolved.used_fallback);
  CHECK(resolved.pipeline != GRANIT_NULL_HANDLE);
  CHECK(resolved.keepalive == fallback);
}
