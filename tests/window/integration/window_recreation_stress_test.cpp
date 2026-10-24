// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "support/soak.h"
#include "support/swapchain_frame.h"
#include <granit/window.hpp>

#include <catch2/catch_all.hpp>

TEST_CASE("同进程反复重建窗口 Surface 与 Swapchain", "[stress][window]") {
  granit::tests::soak_run run;
  {
    granit::window_system system;
    REQUIRE(system.initialize() == granit::result::success);
    granit::renderer renderer;
    REQUIRE(renderer.initialize({.application_name = "granit-window-soak",
                                 .enable_validation = true,
                                 .presentation = granit::presentation_mode::enabled,
                                 .diagnostics = run.diagnostic,
                                 .diagnostic_user_data = &run}) == granit::result::success);
    run.device(renderer);
    while (run.running()) {
      INFO("iteration=" << run.iterations());
      const auto width = 96U + run.random() % 128U;
      const auto height = 72U + run.random() % 128U;
      granit::window window;
      granit::window_desc desc{.title = "Granit Window Soak", .width = width, .height = height};
#if defined(_WIN32)
      // 长测反复创建窗口时不抢占用户桌面，可见窗口流程仍由普通 Smoke 覆盖。
      desc.flags = granit::window_flag::none;
#endif
      REQUIRE(window.initialize(system, desc) == granit::result::success);
      granit::surface surface;
      REQUIRE(window.create_surface(renderer, surface) == granit::result::success);
      granit::swapchain chain;
      REQUIRE(chain.initialize(renderer, surface, {.width = width, .height = height}) ==
              granit::result::success);
      granit::frame_context frames;
      REQUIRE(frames.initialize(renderer) == granit::result::success);
      REQUIRE(system.process_events() == granit::result::success);
      // 长测必须消费事件，避免测试框架自身的队列增长污染内存趋势。
      granit::window_event event;
      granit::result polled;
      while ((polled = system.poll(event)).ok()) {
      }
      REQUIRE(polled == granit::result::not_ready);
      granit::input_event input;
      while ((polled = system.poll(input)).ok()) {
      }
      REQUIRE(polled == granit::result::not_ready);
      granit::swapchain_info info;
      REQUIRE(chain.query_info(info) == granit::result::success);
      REQUIRE(granit::tests::render_clear_frames(chain, frames, info.width, info.height, 3) ==
              granit::result::success);
      REQUIRE(chain.recreate({.width = width, .height = height}) == granit::result::success);
      REQUIRE(chain.query_info(info) == granit::result::success);
      REQUIRE(granit::tests::render_clear_frames(chain, frames, info.width, info.height, 3) ==
              granit::result::success);
      run.iteration();
    }
  }
  REQUIRE(run.iterations() > 0);
  REQUIRE(run.validation_errors.load() == 0);
  REQUIRE(granit::tests::soak_interrupted == 0);
}
