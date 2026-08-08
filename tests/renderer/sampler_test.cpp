// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <catch2/catch_all.hpp>
#include <granit/renderer.hpp>
#include <granit/sampler.hpp>

namespace {
bool unavailable_sampler(granit::result value) {
  return value == granit::result::backend_unavailable ||
         value == granit::result::incompatible_driver ||
         value == granit::result::no_suitable_device;
}

TEST_CASE("Sampler 支持基础状态、比较和独立生命周期", "[sampler]") {
  granit::renderer renderer;
  const auto result = renderer.initialize({.application_name = "granit-sampler-tests"});
  if (unavailable_sampler(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::sampler sampler;
  REQUIRE(sampler.initialize(renderer.native_handle(),
                             {.compare = granit::compare_operation::less_equal}) ==
          granit::result::success);
  const auto handle = sampler.native_handle();
  REQUIRE(sampler.reset() == granit::result::success);
  CHECK(granit_sampler_destroy(renderer.native_handle(), handle) == GRANIT_ERROR_INVALID_HANDLE);
}

TEST_CASE("Sampler 严格验证浮点范围和 Renderer 归属", "[sampler][validation]") {
  granit_sampler_desc desc = GRANIT_SAMPLER_DESC_INIT;
  desc.anisotropy_enabled = 0;
  desc.max_anisotropy = 2.0F;
  granit_sampler sampler = GRANIT_NULL_HANDLE;
  CHECK(granit_sampler_create(UINT64_C(1), &desc, &sampler) == GRANIT_ERROR_INVALID_ARGUMENT);

  granit::renderer first;
  const auto result = first.initialize({.application_name = "granit-sampler-first"});
  if (unavailable_sampler(result))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(result == granit::result::success);
  granit::renderer second;
  REQUIRE(second.initialize({.application_name = "granit-sampler-second"}) ==
          granit::result::success);
  desc = GRANIT_SAMPLER_DESC_INIT;
  desc.anisotropy_enabled = 1;
  desc.max_anisotropy = 1000000.0F;
  CHECK(granit_sampler_create(first.native_handle(), &desc, &sampler) == GRANIT_ERROR_UNSUPPORTED);
  desc = GRANIT_SAMPLER_DESC_INIT;
  REQUIRE(granit_sampler_create(first.native_handle(), &desc, &sampler) == GRANIT_SUCCESS);
  CHECK(granit_sampler_destroy(second.native_handle(), sampler) == GRANIT_ERROR_INVALID_HANDLE);
  CHECK(granit_sampler_destroy(first.native_handle(), sampler) == GRANIT_SUCCESS);
}
} // namespace
