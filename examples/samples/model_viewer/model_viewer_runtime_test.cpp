// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/model_viewer_runtime.h"

#include <catch2/catch_all.hpp>

TEST_CASE("Model Viewer Runtime 统一 Renderer 生命周期") {
  using namespace granit::example::model_viewer;
  application_core core;
  model_loading_session loading;
  model_viewer_runtime runtime(core, loading);

  CHECK(runtime.begin_renderer().ok());
  CHECK(core.phase() == application_phase::renderer_pending);
  CHECK(runtime.renderer_ready().ok());
  CHECK(core.phase() == application_phase::asset_loading);
  CHECK(runtime.loading_status() == model_loading_status::idle);

  runtime.cancel_loading();
  runtime.reset();
  CHECK(core.phase() == application_phase::platform_ready);
  CHECK(runtime.loading_status() == model_loading_status::idle);
}
