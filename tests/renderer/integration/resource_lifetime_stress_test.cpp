// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "support/soak.h"

#include <array>
#include <thread>

#include <catch2/catch_all.hpp>

TEST_CASE("同进程持续验证资源提交回读与 generation 失效", "[stress][resource]") {
  granit::tests::soak_run run;
  {
    granit::renderer renderer;
    REQUIRE(renderer.initialize({.application_name = "granit-resource-soak",
                                 .enable_validation = true,
                                 .diagnostics = run.diagnostic,
                                 .diagnostic_user_data = &run}) == granit::result::success);
    run.device(renderer);
    while (run.running()) {
      INFO("iteration=" << run.iterations());
      std::array<std::byte, 256> expected{};
      for (auto& value : expected)
        value = static_cast<std::byte>(run.random() & 255U);
      {
        granit::texture texture;
        REQUIRE(texture.initialize(renderer, {.format = granit::texture_format::rgba8_unorm,
                                              .usage = granit::texture_usage::transfer_source |
                                                       granit::texture_usage::transfer_destination,
                                              .width = 8,
                                              .height = 8}) == granit::result::success);
        REQUIRE(texture.write(expected, {}, {}) == granit::result::success);
        REQUIRE(texture.reset() == granit::result::success);
      }
      granit::buffer buffer;
      REQUIRE(buffer.initialize(renderer, {.size = expected.size(),
                                           .usage = granit::buffer_usage::transfer_source |
                                                    granit::buffer_usage::transfer_destination,
                                           .location = granit::memory_location::device}) ==
              granit::result::success);
      const auto stale = buffer.native_handle();
      {
        granit::upload_batch upload;
        REQUIRE(upload.initialize(renderer) == granit::result::success);
        REQUIRE(upload.write_buffer(buffer.ref(), 0, expected) == granit::result::success);
        REQUIRE(upload.submit() == granit::result::success);
        granit::readback_batch readback;
        REQUIRE(readback.create(renderer, {.max_result_bytes = expected.size(),
                                           .max_operation_count = 1}) == granit::result::success);
        std::uint32_t index{};
        REQUIRE(readback.read_buffer(buffer.ref(), 0, expected.size(), index) ==
                granit::result::success);
        granit::async_operation operation;
        REQUIRE(readback.submit_async(operation) == granit::result::success);
        granit::async_operation_status status;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds{30};
        do {
          REQUIRE(operation.get_status(status) == granit::result::success);
          if (status.complete())
            break;
          REQUIRE(renderer.process_events() == granit::result::success);
          REQUIRE(std::chrono::steady_clock::now() < deadline);
          std::this_thread::yield();
        } while (true);
        REQUIRE(status.state == granit::async_operation_state::succeeded);
        std::array<std::byte, 256> actual{};
        std::uint64_t size = actual.size();
        REQUIRE(granit::copy_readback_result(operation, index, actual, size) ==
                granit::result::success);
        REQUIRE(actual == expected);
      }
      REQUIRE(buffer.reset() == granit::result::success);
      granit::buffer replacement;
      REQUIRE(
          replacement.initialize(renderer, {.size = 256, .usage = granit::buffer_usage::vertex}) ==
          granit::result::success);
      // 仅此负向调用允许预期的 Granit 句柄诊断，Vulkan 错误仍计为失败。
      run.expect_invalid_buffer.store(true);
      const auto stale_result = granit_buffer_destroy(renderer.native_handle(), stale);
      run.expect_invalid_buffer.store(false);
      REQUIRE(stale_result == GRANIT_ERROR_INVALID_HANDLE);
      REQUIRE(replacement.reset() == granit::result::success);
      REQUIRE(renderer.process_events() == granit::result::success);
      run.iteration();
    }
  }
  REQUIRE(run.iterations() > 0);
  REQUIRE(run.validation_errors.load() == 0);
  REQUIRE(granit::tests::soak_interrupted == 0);
}
