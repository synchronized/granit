// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_hot_reload.h"
#include "support/shader_asset_store.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <chrono>
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

TEST_CASE("材质资产热替换保留解析上下文并在解析失败时回退") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-asset-reload"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized.ok());
  granit::tests::shader_asset_store assets;
  const auto directory = std::filesystem::path{GRANIT_SMOKE_SHADER_DIR};
  const auto vertex_path = directory / "triangle.vert.grshaderobj";
  const auto fragment_path = directory / "triangle.frag.grshaderobj";
  REQUIRE(assets.add(vertex_path));
  REQUIRE(assets.add(fragment_path));
  std::vector<std::byte> library_bytes;
  REQUIRE(assets.build_library(library_bytes));
  granit::shader_library library;
  REQUIRE(library.initialize(renderer, library_bytes) == granit::result::success);
  const auto package = [&] {
    using namespace granit::material;
    material_package_desc desc;
    desc.variants.push_back(
        {.pass = make_feature_id("opaque"),
         .features = {},
         .shaders = {assets.reference(vertex_path), assets.reference(fragment_path)},
         .pipeline = {}});
    material_package result;
    REQUIRE(material_package::build(std::move(desc), result) == package_error::none);
    return result;
  };
  const granit::material::material_pipeline_request request{
      .pass = granit::material::make_feature_id("opaque"),
      .variant = granit::material::make_variant_key({}),
      .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
  std::shared_ptr<granit::material::material_runtime_template> fallback;
  REQUIRE(granit::material::material_runtime_template::create(renderer.native_handle(), package(),
                                                              fallback, library.native_handle()) ==
          GRANIT_SUCCESS);
  granit::material::material_hot_reload_slot slot{fallback};
  REQUIRE(slot.reload(renderer.native_handle(), package(), library.native_handle()).result ==
          GRANIT_SUCCESS);
  auto old_snapshot = slot.snapshot();
  REQUIRE(slot.resolve_pipeline(request).result == GRANIT_SUCCESS);

  // Pipeline 延迟创建时仍必须能使用模板借用的 Library。
  REQUIRE(slot.reload(renderer.native_handle(), package()).result == GRANIT_SUCCESS);
  const auto failed = slot.resolve_pipeline(request);
  CHECK(failed.result == GRANIT_SUCCESS);
  CHECK(failed.used_fallback);
  CHECK(failed.primary_result == GRANIT_ERROR_NOT_READY);
  REQUIRE(slot.reload(renderer.native_handle(), package(), library.native_handle()).result ==
          GRANIT_SUCCESS);
  CHECK_FALSE(slot.resolve_pipeline(request).used_fallback);
  granit_graphics_pipeline old_pipeline = GRANIT_NULL_HANDLE;
  REQUIRE(old_snapshot->gpu().acquire_pipeline(request, old_pipeline) == GRANIT_SUCCESS);
  CHECK(old_pipeline != GRANIT_NULL_HANDLE);
}

TEST_CASE("测试资产存储允许按目标构建单后端 Library 并缓存清单元数据") {
  const auto source = std::filesystem::path{GRANIT_SMOKE_SHADER_DIR} / "triangle.vert.grshaderobj";
  const auto folder = std::filesystem::temp_directory_path() /
                      ("granit-shader-asset-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  REQUIRE(std::filesystem::create_directory(folder));
  struct cleanup {
    std::filesystem::path folder;
    ~cleanup() {
      std::error_code ignored;
      std::filesystem::remove(folder / "vertex.grshaderobj.spv", ignored);
      std::filesystem::remove(folder / "vertex.grshaderobj", ignored);
      std::filesystem::remove(folder, ignored);
    }
  } guard{folder};
  const auto path = folder / "vertex.grshaderobj";
  REQUIRE(std::filesystem::copy_file(source, path));
  REQUIRE(std::filesystem::copy_file(source.string() + ".spv", path.string() + ".spv"));
  granit::tests::shader_asset_store assets;
  REQUIRE(assets.add(path));
  REQUIRE(assets.add(path));
  const auto reference = assets.reference(path);
  // reference 使用已验证的缓存，不再次读取可能已卸载的资产文件。
  REQUIRE(std::filesystem::remove(path));
  CHECK(assets.reference(path).asset_id == reference.asset_id);
  std::vector<std::byte> library_bytes;
  CHECK(assets.build_library(library_bytes, GRANIT_SHADER_BACKEND_VULKAN_BIT));
  granit::shader_library_info info;
  REQUIRE(granit::inspect_shader_library(library_bytes, info) == granit::result::success);
  CHECK(info.backends == granit::shader_backend::vulkan);
  CHECK_FALSE(assets.build_library(library_bytes, GRANIT_SHADER_BACKEND_WEBGPU_BIT));
  CHECK_FALSE(assets.add(folder / "missing.grshaderobj"));
}
