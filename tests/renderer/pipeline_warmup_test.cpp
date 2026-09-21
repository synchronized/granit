// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/pipeline_warmup.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/shader.hpp>

#include <catch2/catch_all.hpp>

#include <array>
#include <chrono>
#include <cstddef>
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

std::vector<std::byte> load_binary(const char* name) {
  std::ifstream stream{std::string{GRANIT_TEST_ASSET_DIR} + "/" + name, std::ios::binary};
  const std::vector<char> input{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> output(input.size());
  for (std::size_t index = 0; index < input.size(); ++index)
    output[index] = static_cast<std::byte>(input[index]);
  return output;
}

void await(granit::renderer& renderer, granit::async_operation& operation) {
  for (std::uint32_t iteration = 0; iteration < 1000; ++iteration) {
    granit::async_operation_status status;
    REQUIRE(operation.get_status(status) == granit::result::success);
    if (status.state == granit::async_operation_state::succeeded)
      return;
    REQUIRE(status.state == granit::async_operation_state::running);
    REQUIRE(renderer.process_events() == granit::result::success);
    std::this_thread::sleep_for(std::chrono::milliseconds{1});
  }
  FAIL("Pipeline 预热操作未在限定轮询次数内完成");
}

TEST_CASE("Pipeline 预热批次拒绝空提交和越界", "[pipeline-warmup][contract]") {
  granit::pipeline_warmup_batch batch;
  CHECK(batch.create(GRANIT_NULL_HANDLE) == granit::result::invalid_argument);
}

TEST_CASE("Compute Pipeline 预热提供稳定键和缓存命中", "[pipeline-warmup][compute]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-pipeline-warmup"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);
  granit::renderer_limits limits;
  REQUIRE(renderer.get_limits(limits) == granit::result::success);
  REQUIRE(limits.supports_non_blocking_pipeline_warmup());

  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);
  const auto spirv = load_binary("minimal.comp.spv");
  REQUIRE_FALSE(spirv.empty());
  granit::shader shader;
  REQUIRE(shader.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::compute, .code = spirv}) ==
          granit::result::success);

  granit_compute_pipeline_desc desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  desc.layout = layout.native_handle();
  desc.compute_shader = shader.native_handle();
  std::array<std::byte, GRANIT_PIPELINE_WARMUP_CACHE_KEY_SIZE> first_key{};
  for (int pass = 0; pass < 2; ++pass) {
    granit::pipeline_warmup_batch batch;
    REQUIRE(batch.create(renderer, {.max_operation_count = 1}) == granit::result::success);
    CHECK(batch.ref().native_handle() == batch.native_handle());
    std::uint32_t index{};
    REQUIRE(batch.add_compute(desc, index) == granit::result::success);
    REQUIRE(index == 0);
    granit::async_operation operation;
    REQUIRE(batch.submit_async(operation) == granit::result::success);
    granit::async_operation_status initial_status;
    REQUIRE(operation.get_status(initial_status) == granit::result::success);
    if (pass == 0)
      REQUIRE(initial_status.state == granit::async_operation_state::running);
    await(renderer, operation);
    granit::pipeline_warmup_result_info info;
    REQUIRE(granit::get_pipeline_warmup_result(operation, 0, info) == granit::result::success);
    CHECK(info.operation_result == granit::result::success);
    CHECK(info.cache_hit == (pass != 0));
    if (pass == 0)
      first_key = info.cache_key;
    else
      CHECK(info.cache_key == first_key);
  }
}

TEST_CASE("Pipeline 预热操作保留已经提交的资源", "[pipeline-warmup][lifetime]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-warmup-lifetime"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);
  granit::shader shader;
  REQUIRE(shader.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::compute,
                             .code = load_binary("minimal.comp.spv")}) ==
          granit::result::success);
  granit_compute_pipeline_desc desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  desc.layout = layout.native_handle();
  desc.compute_shader = shader.native_handle();
  granit::pipeline_warmup_batch batch;
  REQUIRE(batch.create(renderer.native_handle(), {.max_operation_count = 1}) ==
          granit::result::success);
  std::uint32_t index{};
  REQUIRE(batch.add_compute(desc, index) == granit::result::success);
  granit::async_operation operation;
  REQUIRE(batch.submit_async(operation) == granit::result::success);

  REQUIRE(shader.reset() == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
  await(renderer, operation);
  granit::pipeline_warmup_result_info info;
  REQUIRE(granit::get_pipeline_warmup_result(operation, index, info) == granit::result::success);
  CHECK(info.operation_result == granit::result::success);
}

TEST_CASE("Pipeline 预热取消收敛终态并安全释放资源", "[pipeline-warmup][cancel]") {
  granit::renderer renderer;
  const auto initialized = renderer.initialize({.application_name = "granit-warmup-cancel"});
  if (environment_unavailable(initialized))
    SKIP("当前运行环境没有满足要求的 Vulkan 设备");
  REQUIRE(initialized == granit::result::success);

  granit::pipeline_layout layout;
  REQUIRE(layout.initialize(renderer.native_handle()) == granit::result::success);
  granit::shader shader;
  REQUIRE(shader.initialize(renderer.native_handle(),
                            {.stage = granit::shader_stage::compute,
                             .code = load_binary("minimal.comp.spv")}) ==
          granit::result::success);
  granit_compute_pipeline_desc desc = GRANIT_COMPUTE_PIPELINE_DESC_INIT;
  desc.layout = layout.native_handle();
  desc.compute_shader = shader.native_handle();
  granit::pipeline_warmup_batch batch;
  REQUIRE(batch.create(renderer.native_handle(), {.max_operation_count = 32}) ==
          granit::result::success);
  for (std::uint32_t entry = 0; entry < 32; ++entry) {
    std::uint32_t index{};
    REQUIRE(batch.add_compute(desc, index) == granit::result::success);
  }
  granit::async_operation operation;
  REQUIRE(batch.submit_async(operation) == granit::result::success);
  REQUIRE(operation.request_cancel() == granit::result::success);
  granit::async_operation_status status;
  for (std::uint32_t iteration = 0; iteration < 1000; ++iteration) {
    REQUIRE(operation.get_status(status) == granit::result::success);
    if (status.complete())
      break;
    REQUIRE(renderer.process_events() == granit::result::success);
  }
  REQUIRE(status.complete());
  CHECK((status.state == granit::async_operation_state::succeeded ||
         status.state == granit::async_operation_state::cancelled));
  REQUIRE(operation.reset() == granit::result::success);
  REQUIRE(batch.reset_handle() == granit::result::success);
  REQUIRE(shader.reset() == granit::result::success);
  REQUIRE(layout.reset() == granit::result::success);
}

} // namespace
