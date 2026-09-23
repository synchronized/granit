// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "application/application.h"

#include <granit/renderer/frame_context.hpp>

#include <cstdint>
#include <iostream>
#include <span>
#include <string_view>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

namespace {

int report_failure(std::string_view operation, granit::result result) {
  std::cerr << operation << " failed: " << result.message() << "\n";
  return 1;
}

class clear_application final : public granit::example::application {
private:
  granit::result on_initialize() noexcept override {
    return frame_context_.initialize(renderer());
  }

  granit::result on_render(granit::example::present_frame& frame) noexcept override {
    granit::frame_recording recording;
    auto operation = frame_context_.begin(frame.acquired, recording);

    const granit::color_attachment_desc color{
        .view = frame.backbuffer.view,
        .resolve_view = {},
        .clear_value = {.red = 0.04F, .green = 0.03F, .blue = 0.05F, .alpha = 1.0F}};
    const granit::rendering_desc rendering{
        .color_attachments = std::span{&color, 1},
        .area = {0, 0, frame.swapchain.width, frame.swapchain.height}};
    if (operation.ok())
      operation = recording.recorder().begin_rendering(rendering);
    if (operation.ok())
      operation = recording.recorder().end_rendering();
    if (operation.ok())
      operation = recording.submit();
    if (operation.failed() && recording.valid())
      static_cast<void>(recording.abort());
    return operation;
  }

  void on_shutdown(granit::result) noexcept override {
    static_cast<void>(frame_context_.reset());
  }

  granit::frame_context frame_context_;
};

clear_application application;

} // namespace

#if defined(__EMSCRIPTEN__)
extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_rendered_frames() noexcept {
  return application.rendered_frames();
}

extern "C" EMSCRIPTEN_KEEPALIVE std::uint32_t granit_tutorial_01_recreate_count() noexcept {
  return application.completed_recreates();
}

extern "C" EMSCRIPTEN_KEEPALIVE int granit_tutorial_01_ready() noexcept {
  return application.ready() ? 1 : 0;
}
#endif

int main(int argument_count, char** arguments) {
  const bool smoke_test = argument_count == 2 && std::string_view{arguments[1]} == "--smoke-test";
  const auto result = application.run({.title = "Granit Tutorial 01",
                                       .application_name = "Granit Tutorial 01",
                                       .smoke_test = smoke_test});
  if (smoke_test && result == granit::result::backend_unavailable)
    return 77;
  return result.failed() ? report_failure("application run", result) : 0;
}
