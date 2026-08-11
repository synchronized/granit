// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "material/material_template_gpu.h"

#include <granit/renderer/renderer.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <barrier>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>
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
  if (bytes.empty() || bytes.size() % sizeof(std::uint32_t) != 0) {
    return {};
  }
  std::vector<std::uint32_t> words(bytes.size() / sizeof(std::uint32_t));
  std::memcpy(words.data(), bytes.data(), bytes.size());
  return words;
}

granit::material::material_package build_package() {
  using namespace granit::material;
  material_package_desc desc;
  desc.variants.push_back({.pass = make_feature_id("opaque"),
                           .features = {},
                           .shaders = {{.stage = package_shader_stage::vertex,
                                        .entry_point = "main",
                                        .spirv = load_shader("minimal.vert.spv")},
                                       {.stage = package_shader_stage::fragment,
                                        .entry_point = "main",
                                        .spirv = load_shader("minimal.frag.spv")}}});
  material_package package;
  REQUIRE(material_package::build(std::move(desc), package) == package_error::none);
  return package;
}

} // namespace

TEST_CASE("材质模板延迟创建并复用 Graphics Pipeline") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-material-template"});
  if (environment_unavailable(initialized)) {
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  }
  REQUIRE(initialized == granit::result::success);

  const auto package = build_package();
  granit::material::material_template_gpu material;
  REQUIRE(material.initialize(renderer.native_handle(), package) == GRANIT_SUCCESS);
  REQUIRE(material.material_layout() != GRANIT_NULL_HANDLE);
  REQUIRE(material.pipeline_layout() != GRANIT_NULL_HANDLE);

  const granit::material::material_pipeline_request request{
      .pass = granit::material::make_feature_id("opaque"),
      .variant = granit::material::make_variant_key({}),
      .color_format = GRANIT_TEXTURE_FORMAT_RGBA8_UNORM};
  constexpr std::size_t thread_count = 8;
  std::barrier start{thread_count};
  std::array<granit_graphics_pipeline, thread_count> pipelines{};
  std::array<granit_result, thread_count> results{};
  std::vector<std::thread> workers;
  workers.reserve(thread_count);
  for (std::size_t index = 0; index < thread_count; ++index) {
    workers.emplace_back([&, index] {
      start.arrive_and_wait();
      results[index] = material.acquire_pipeline(request, pipelines[index]);
    });
  }
  for (auto& worker : workers) {
    worker.join();
  }
  for (std::size_t index = 0; index < thread_count; ++index) {
    REQUIRE(results[index] == GRANIT_SUCCESS);
    CHECK(pipelines[index] == pipelines.front());
  }
  const auto first = pipelines.front();
  REQUIRE(first != GRANIT_NULL_HANDLE);
  CHECK(material.cached_pipeline_count() == 1);

  granit_graphics_pipeline reused = GRANIT_NULL_HANDLE;
  REQUIRE(material.acquire_pipeline(request, reused) == GRANIT_SUCCESS);
  CHECK(reused == first);
  CHECK(material.cached_pipeline_count() == 1);

  auto missing = request;
  missing.variant += 1;
  CHECK(material.acquire_pipeline(missing, reused) == GRANIT_ERROR_NOT_READY);
  CHECK(reused == GRANIT_NULL_HANDLE);
  CHECK(material.reset() == GRANIT_SUCCESS);
}
