// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/renderer.hpp>
#include <granit/renderer/shader.hpp>

#include "shader_asset.h"

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include <catch2/catch_all.hpp>

namespace {

TEST_CASE("Shader包装把空Renderer归类为无效句柄", "[shader][contract]") {
  granit::shader shader;
  CHECK(shader.initialize(GRANIT_NULL_HANDLE, {}) == granit::result::invalid_handle);
  CHECK(shader.initialize_asset(GRANIT_NULL_HANDLE, {}) == granit::result::invalid_handle);
}

constexpr std::array vertex_spirv{
    UINT32_C(0x07230203), UINT32_C(0x00010000), UINT32_C(0x000d000b), UINT32_C(0x00000015),
    UINT32_C(0x00000000), UINT32_C(0x00020011), UINT32_C(0x00000001), UINT32_C(0x0006000b),
    UINT32_C(0x00000001), UINT32_C(0x4c534c47), UINT32_C(0x6474732e), UINT32_C(0x3035342e),
    UINT32_C(0x00000000), UINT32_C(0x0003000e), UINT32_C(0x00000000), UINT32_C(0x00000001),
    UINT32_C(0x0006000f), UINT32_C(0x00000000), UINT32_C(0x00000004), UINT32_C(0x6e69616d),
    UINT32_C(0x00000000), UINT32_C(0x0000000d), UINT32_C(0x00030047), UINT32_C(0x0000000b),
    UINT32_C(0x00000002), UINT32_C(0x00050048), UINT32_C(0x0000000b), UINT32_C(0x00000000),
    UINT32_C(0x0000000b), UINT32_C(0x00000000), UINT32_C(0x00050048), UINT32_C(0x0000000b),
    UINT32_C(0x00000001), UINT32_C(0x0000000b), UINT32_C(0x00000001), UINT32_C(0x00050048),
    UINT32_C(0x0000000b), UINT32_C(0x00000002), UINT32_C(0x0000000b), UINT32_C(0x00000003),
    UINT32_C(0x00050048), UINT32_C(0x0000000b), UINT32_C(0x00000003), UINT32_C(0x0000000b),
    UINT32_C(0x00000004), UINT32_C(0x00020013), UINT32_C(0x00000002), UINT32_C(0x00030021),
    UINT32_C(0x00000003), UINT32_C(0x00000002), UINT32_C(0x00030016), UINT32_C(0x00000006),
    UINT32_C(0x00000020), UINT32_C(0x00040017), UINT32_C(0x00000007), UINT32_C(0x00000006),
    UINT32_C(0x00000004), UINT32_C(0x00040015), UINT32_C(0x00000008), UINT32_C(0x00000020),
    UINT32_C(0x00000000), UINT32_C(0x0004002b), UINT32_C(0x00000008), UINT32_C(0x00000009),
    UINT32_C(0x00000001), UINT32_C(0x0004001c), UINT32_C(0x0000000a), UINT32_C(0x00000006),
    UINT32_C(0x00000009), UINT32_C(0x0006001e), UINT32_C(0x0000000b), UINT32_C(0x00000007),
    UINT32_C(0x00000006), UINT32_C(0x0000000a), UINT32_C(0x0000000a), UINT32_C(0x00040020),
    UINT32_C(0x0000000c), UINT32_C(0x00000003), UINT32_C(0x0000000b), UINT32_C(0x0004003b),
    UINT32_C(0x0000000c), UINT32_C(0x0000000d), UINT32_C(0x00000003), UINT32_C(0x00040015),
    UINT32_C(0x0000000e), UINT32_C(0x00000020), UINT32_C(0x00000001), UINT32_C(0x0004002b),
    UINT32_C(0x0000000e), UINT32_C(0x0000000f), UINT32_C(0x00000000), UINT32_C(0x0004002b),
    UINT32_C(0x00000006), UINT32_C(0x00000010), UINT32_C(0x00000000), UINT32_C(0x0004002b),
    UINT32_C(0x00000006), UINT32_C(0x00000011), UINT32_C(0x3f800000), UINT32_C(0x0007002c),
    UINT32_C(0x00000007), UINT32_C(0x00000012), UINT32_C(0x00000010), UINT32_C(0x00000010),
    UINT32_C(0x00000010), UINT32_C(0x00000011), UINT32_C(0x00040020), UINT32_C(0x00000013),
    UINT32_C(0x00000003), UINT32_C(0x00000007), UINT32_C(0x00050036), UINT32_C(0x00000002),
    UINT32_C(0x00000004), UINT32_C(0x00000000), UINT32_C(0x00000003), UINT32_C(0x000200f8),
    UINT32_C(0x00000005), UINT32_C(0x00050041), UINT32_C(0x00000013), UINT32_C(0x00000014),
    UINT32_C(0x0000000d), UINT32_C(0x0000000f), UINT32_C(0x0003003e), UINT32_C(0x00000014),
    UINT32_C(0x00000012), UINT32_C(0x000100fd), UINT32_C(0x00010038)};

bool shader_environment_unavailable(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Shader 创建后不依赖 SPIR-V 输入内存", "[shader]") {
  granit::renderer renderer;
  const auto renderer_result =
      renderer.initialize({.application_name = "granit-shader-tests", .enable_validation = true});
  if (renderer_result == granit::result::unsupported)
    SKIP("当前运行环境没有 Khronos validation layer");
  if (shader_environment_unavailable(renderer_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(renderer_result == granit::result::success);

  auto code = vertex_spirv;
  granit::shader shader;
  REQUIRE(shader.initialize(renderer.native_handle(), {.stage = granit::shader_stage::vertex,
                                                       .code = std::as_bytes(std::span{code})}) ==
          granit::result::success);
  code.fill(0);
  const auto handle = shader.native_handle();
  REQUIRE(shader.reset() == granit::result::success);
  CHECK(granit_shader_destroy(renderer.native_handle(), handle) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Shader 校验 SPIR-V、入口点和 Renderer domain", "[shader][validation]") {
  granit_shader_desc desc = GRANIT_SHADER_DESC_INIT;
  granit_shader shader = GRANIT_NULL_HANDLE;
  auto invalid = vertex_spirv;
  invalid[0] = 0;
  desc.code = invalid.data();
  desc.code_size = sizeof(invalid);
  CHECK(granit_shader_create(UINT64_C(1), &desc, &shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.code = vertex_spirv.data();
  desc.code_size = sizeof(vertex_spirv) - 1;
  CHECK(granit_shader_create(UINT64_C(1), &desc, &shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.code_size = sizeof(vertex_spirv);
  desc.entry_point_length = 0;
  CHECK(granit_shader_create(UINT64_C(1), &desc, &shader) == GRANIT_ERROR_INVALID_ARGUMENT);
  desc.entry_point_length = 4;
  desc.stage = UINT32_C(99);
  CHECK(granit_shader_create(UINT64_C(1), &desc, &shader) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-shader-first"});
  if (shader_environment_unavailable(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-shader-second"}) ==
          granit::result::success);
  desc = GRANIT_SHADER_DESC_INIT;
  desc.code = vertex_spirv.data();
  desc.code_size = sizeof(vertex_spirv);
  REQUIRE(granit_shader_create(first.native_handle(), &desc, &shader) == GRANIT_SUCCESS);
  CHECK(granit_shader_destroy(second.native_handle(), shader) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_shader_destroy(first.native_handle(), shader) == GRANIT_SUCCESS);
}

TEST_CASE("Shader Asset 按 Renderer 后端验证并创建 Shader", "[shader][asset]") {
  const auto spirv = std::as_bytes(std::span{vertex_spirv});
  constexpr std::string_view wgsl =
      "@vertex fn main() -> @builtin(position) vec4f { return vec4f(); }";
  std::vector<std::byte> manifest;
  REQUIRE(granit::tools::encode_shader_asset(
              {wgsl, spirv, "{}", {}, 1, 0, GRANIT_SHADER_STAGE_VERTEX, "main"}, manifest) ==
          granit::tools::shader_asset_error::success);

  granit::shader invalid;
  CHECK(invalid.initialize_packaged_asset(
            GRANIT_NULL_HANDLE, {.manifest = manifest, .sidecar = spirv}) ==
        granit::result::invalid_handle);

  granit::renderer renderer;
  const auto renderer_result = renderer.initialize({.application_name = "granit-shader-asset"});
  if (shader_environment_unavailable(renderer_result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(renderer_result == granit::result::success);

  granit::shader shader;
  REQUIRE(shader.initialize_packaged_asset(renderer.native_handle(),
                                           {.manifest = manifest, .sidecar = spirv}) ==
          granit::result::success);
  REQUIRE(shader.reset() == granit::result::success);

  auto damaged = std::vector<std::byte>{spirv.begin(), spirv.end()};
  damaged.front() ^= std::byte{1};
  CHECK(shader.initialize_packaged_asset(renderer.native_handle(),
                                         {.manifest = manifest, .sidecar = damaged}) ==
        granit::result::invalid_argument);
}

} // namespace
