// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include <granit/renderer.h>

#include "core/handle_table.h"
#include "renderer/renderer_state.h"

namespace granit::detail {

/** 线程安全地管理进程内公开 renderer 句柄。 */
class renderer_registry {
public:
  static renderer_registry& instance();

  [[nodiscard]] granit_result create(
    std::string_view application_name,
    bool enable_validation,
    granit_renderer& renderer);
  [[nodiscard]] granit_result destroy(granit_renderer renderer);
  [[nodiscard]] std::shared_ptr<renderer_state> acquire(granit_renderer renderer);

private:
  renderer_registry() = default;

  [[nodiscard]] std::uint32_t allocate_domain() noexcept;

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<renderer_state>> renderers_;
  std::uint32_t next_domain_{1};
};

} // namespace granit::detail

#endif
