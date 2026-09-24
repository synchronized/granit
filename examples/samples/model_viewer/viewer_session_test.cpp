// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer/viewer_session.h"

#include <catch2/catch_all.hpp>

TEST_CASE("Viewer Session 拥有并统一 Renderer 与加载生命周期") {
  using namespace granit::example::model_viewer;
  viewer_session session;

  CHECK(session.begin_renderer().ok());
  CHECK(session.phase() == application_phase::renderer_pending);
  CHECK(session.renderer_ready().ok());
  CHECK(session.phase() == application_phase::asset_loading);
  CHECK(session.loading_status() == model_loading_status::idle);

  session.cancel_loading();
  session.reset();
  CHECK(session.phase() == application_phase::platform_ready);
  CHECK(session.loading_status() == model_loading_status::idle);
}
