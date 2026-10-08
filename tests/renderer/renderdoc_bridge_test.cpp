// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/vulkan/renderdoc_bridge.h"

#include "core/diagnostic_sink.h"
#include "core/frame_trace.h"

int main() {
  granit::detail::diagnostic_sink diagnostics;
  granit::detail::renderdoc_bridge bridge;
  bridge.initialize(diagnostics);
  bridge.trigger_initial_capture();
  granit::detail::frame_trace::instance().emit_diagnostic(
      granit::detail::diagnostic_severity::info,
      granit::detail::diagnostic_category::general,
      "RenderDoc bridge test completed");
  return 0;
}
