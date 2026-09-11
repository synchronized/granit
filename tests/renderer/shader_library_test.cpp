// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/shader_library.hpp>

#include "assets/shader_asset.h"
#include "assets/shader_library.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

namespace {

std::vector<std::byte> make_library() {
  using namespace granit::tools;
  constexpr std::string_view wgsl = "@compute @workgroup_size(1) fn main() {}\n";
  constexpr std::array spirv{std::byte{3}, std::byte{2}, std::byte{35}, std::byte{7}};
  constexpr std::string_view reflection = "{\"schema\":1}\n";
  const auto wgsl_bytes = std::span{reinterpret_cast<const std::byte*>(wgsl.data()), wgsl.size()};
  std::vector<std::byte> manifest;
  const auto cache_key =
      make_shader_cache_key({wgsl, "wgsl", "main", "compute", "tint-r1", "vulkan1.3", ""});
  REQUIRE(encode_shader_asset(
              {wgsl, spirv, reflection, cache_key, 3, 0, GRANIT_SHADER_STAGE_COMPUTE, "main"},
              manifest) == shader_asset_error::success);
  const std::array sources{shader_library_asset_source{manifest, wgsl_bytes, spirv}};
  std::vector<std::byte> archive;
  REQUIRE(encode_shader_library({sources, shader_library_backend_all}, archive) ==
          shader_library_error::success);
  return archive;
}

std::vector<std::byte> read_binary(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary | std::ios::ate};
  REQUIRE(stream.good());
  const auto size = stream.tellg();
  REQUIRE(size > 0);
  std::vector<std::byte> bytes(static_cast<std::size_t>(size));
  stream.seekg(0);
  stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
  REQUIRE(stream.good());
  return bytes;
}

std::vector<std::byte> make_runtime_library(granit::tools::shader_cache_key& content_id) {
  using namespace granit::tools;
  const auto directory = std::filesystem::path{GRANIT_TEST_ASSET_DIR};
  const auto manifest = read_binary(directory / "minimal.vert.grshader");
  const auto wgsl = read_binary(directory / "minimal.vert.grshader.wgsl");
  const auto spirv = read_binary(directory / "minimal.vert.grshader.spv");
  shader_asset_view asset;
  REQUIRE(decode_shader_asset(manifest, asset) == shader_asset_error::success);
  content_id = asset.content_id;
  const std::array sources{shader_library_asset_source{manifest, wgsl, spirv}};
  std::vector<std::byte> archive;
  REQUIRE(encode_shader_library({sources, shader_library_backend_all}, archive) ==
          shader_library_error::success);
  return archive;
}

bool environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Shader Library 检查返回稳定摘要", "[shader_library][inspect]") {
  const auto archive = make_library();
  granit_shader_library_info native = GRANIT_SHADER_LIBRARY_INFO_INIT;
  REQUIRE(granit_shader_library_inspect(archive.data(), archive.size(), &native) == GRANIT_SUCCESS);
  CHECK(native.backend_flags ==
        (GRANIT_SHADER_LIBRARY_BACKEND_VULKAN_BIT | GRANIT_SHADER_LIBRARY_BACKEND_WEBGPU_BIT));
  CHECK(native.shader_count == 1);
  CHECK(native.variant_count == 2);
  CHECK(native.payload_count == 2);
  CHECK(native.archive_size == archive.size());

  granit::shader_library_info info;
  REQUIRE(granit::inspect_shader_library(archive, info) == granit::result::success);
  CHECK(info.shader_count == native.shader_count);
  CHECK(info.variant_count == native.variant_count);
  CHECK(info.payload_count == native.payload_count);

  auto corrupted = archive;
  corrupted.back() ^= std::byte{1};
  CHECK(granit_shader_library_inspect(corrupted.data(), corrupted.size(), &native) ==
        GRANIT_ERROR_INVALID_ARGUMENT);
  auto unsupported = archive;
  unsupported[8] = std::byte{2};
  CHECK(granit_shader_library_inspect(unsupported.data(), unsupported.size(), &native) ==
        GRANIT_ERROR_UNSUPPORTED);
}

TEST_CASE("Shader Library 句柄校验类型、domain 和 generation", "[shader_library][lifecycle]") {
  const auto archive = make_library();
  granit::shader_library invalid;
  CHECK(invalid.initialize(GRANIT_NULL_HANDLE, archive) == granit::result::invalid_handle);

  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-shader-library-first"});
  if (environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-shader-library-second"}) ==
          granit::result::success);

  granit::shader_library library;
  REQUIRE(library.initialize(first.native_handle(), archive) == granit::result::success);
  const auto handle = library.native_handle();
  granit::shader_library_info info;
  REQUIRE(library.get_info(info) == granit::result::success);
  CHECK(info.shader_count == 1);
  granit_shader_library_info native = GRANIT_SHADER_LIBRARY_INFO_INIT;
  CHECK(granit_shader_library_get_info(second.native_handle(), handle, &native) ==
        GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_shader_destroy(first.native_handle(), handle) == GRANIT_ERROR_INVALID_HANDLE);
  REQUIRE(library.reset() == granit::result::success);
  CHECK(granit_shader_library_destroy(first.native_handle(), handle) ==
        GRANIT_ERROR_INVALID_HANDLE);

  granit::shader_library replacement;
  REQUIRE(replacement.initialize(first.native_handle(), archive) == granit::result::success);
  CHECK(replacement.native_handle() != handle);
  REQUIRE(first.reset() == granit::result::success);
  CHECK(replacement.get_info(info) == granit::result::invalid_handle);
  CHECK(replacement.reset() == granit::result::invalid_handle);
}

TEST_CASE("Shader Library 由 Renderer 选择载荷并共享后端 Shader", "[shader_library][selection]") {
  granit::tools::shader_cache_key content_id{};
  auto archive = make_runtime_library(content_id);
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-shader-library"});
  if (environment_unavailable(renderer_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(renderer_result == granit::result::success);

  granit::shader_library library;
  granit::shader first;
  CHECK(library.create_shader(content_id, first) == granit::result::invalid_handle);
  REQUIRE(library.initialize(renderer.native_handle(), archive) == granit::result::success);
  granit::shader second;
  REQUIRE(library.create_shader(content_id, first) == granit::result::success);
  CHECK(library.create_shader(content_id, first) == granit::result::invalid_argument);
  REQUIRE(library.create_shader(content_id, second) == granit::result::success);
  CHECK(first.native_handle() != second.native_handle());
  CHECK(library.reset() == granit::result::resource_in_use);
  REQUIRE(first.reset() == granit::result::success);
  CHECK(library.reset() == granit::result::resource_in_use);
  REQUIRE(second.reset() == granit::result::success);
  REQUIRE(library.reset() == granit::result::success);

  REQUIRE(library.initialize(renderer.native_handle(), archive) == granit::result::success);
  auto missing = content_id;
  missing[0] ^= std::byte{1};
  CHECK(library.create_shader(missing, first) == granit::result::not_ready);
  archive.back() ^= std::byte{1};
  CHECK(library.create_shader(content_id, first) == granit::result::invalid_argument);
}

} // namespace
