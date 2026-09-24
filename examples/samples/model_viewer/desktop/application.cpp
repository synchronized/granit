// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/desktop/application.h"

#include "model_viewer/viewer_application.h"

#include <granit/core/result.hpp>

#include <iostream>
#include <string_view>
#include <utility>

namespace {

class desktop_observer final : public granit::example::model_viewer::viewer_application_observer {
public:
  void on_status(granit::example::model_viewer::viewer_application_status status,
                 std::string_view stage, granit::result result) noexcept override {
    if (status == granit::example::model_viewer::viewer_application_status::failed) {
      std::cerr << "模型查看器在 " << stage << " 阶段失败：" << granit::result_message(result)
                << '\n';
    }
  }

  void on_diagnostic(granit::diagnostic_severity severity,
                     std::string_view message) noexcept override {
    auto& output = severity == granit::diagnostic_severity::info ? std::cout : std::cerr;
    output << "[granit] " << message << '\n';
  }
};

} // namespace

granit::example::model_viewer::desktop::application::application(options options)
    : options_(std::move(options)) {}

int granit::example::model_viewer::desktop::application::run() {
  const auto& options = options_;
  const auto profiling = !options.profile_output_path.empty();
  desktop_observer observer;
  granit::example::model_viewer::viewer_application application;
  const auto result = application.run({
      .host = {.executable_path = options.asset_path,
               .title = "Granit Model Viewer",
               .window_system = {.backend = granit::window_backend::sdl3},
               .window_flags = granit::window_flag::visible | granit::window_flag::resizable |
                               granit::window_flag::high_dpi,
               .width = profiling ? 1920U : 1280U,
               .height = profiling ? 1080U : 720U},
      .model_location = options.asset_path,
      .environment_location = options.environment_path,
      .profile_output_path = options.profile_output_path,
      .renderer_backend = options.backend,
      .present_mode = options.presentation,
      .execution = viewer_execution_mode::dedicated_thread,
      .initial_quality = {.sample_count = GRANIT_SAMPLE_COUNT_4,
                          .enable_fxaa = true,
                          .enable_specular_aa = true,
                          .sampler_anisotropy = 8.0F},
      .observer = &observer,
      .enable_validation = options.enable_validation,
      .show_ui = options.show_ui,
      .smoke_test = options.smoke_test,
  });
  return result.failed() ? 1 : 0;
}
