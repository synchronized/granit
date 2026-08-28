// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEB_RENDERER_REGISTRY_H_
#define GRANIT_WEB_RENDERER_REGISTRY_H_

#include <memory>
#include <mutex>
#include <unordered_map>

#include <granit/renderer/renderer.h>

#include "backend/webgpu/renderer_state.h"
#include "core/handle_table.h"

namespace granit::detail {

/** Emscripten 平台的 Renderer 根句柄表；资源接口迁移后由通用 Registry 取代。 */
class web_renderer_registry {
public:
  static web_renderer_registry& instance();

  [[nodiscard]] granit_result create(granit_diagnostic_callback diagnostic_callback,
                                     void* diagnostic_user_data, granit_renderer& renderer);
  [[nodiscard]] granit_result destroy(granit_renderer renderer);
  [[nodiscard]] granit_result get_limits(granit_renderer renderer, granit_renderer_limits& limits);
  [[nodiscard]] granit_result get_status(granit_renderer renderer, granit_renderer_status& status);
  [[nodiscard]] granit_result process_events(granit_renderer renderer);
  [[nodiscard]] std::shared_ptr<webgpu_renderer_state> acquire(granit_renderer renderer);

private:
  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<webgpu_renderer_state>> renderers_;
};

} // namespace granit::detail

#endif
