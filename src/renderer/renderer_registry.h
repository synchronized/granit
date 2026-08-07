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
#include <granit/surface.h>

#include "core/handle_table.h"
#include "renderer/renderer_state.h"

namespace granit::detail {

/** 线程安全地管理进程内公开 renderer 句柄。 */
class renderer_registry {
public:
  static renderer_registry& instance();

  [[nodiscard]] granit_result create(std::string_view application_name, bool enable_validation,
                                     std::uint32_t surface_types, granit_renderer& renderer);
  [[nodiscard]] granit_result destroy(granit_renderer renderer);
  [[nodiscard]] std::shared_ptr<renderer_state> acquire(granit_renderer renderer);
  [[nodiscard]] granit_result create_win32_surface(granit_renderer renderer, void* native_instance,
                                                   void* native_window, granit_surface& surface);
  [[nodiscard]] granit_result destroy_surface(granit_renderer renderer, granit_surface surface);

private:
  renderer_registry() = default;

  [[nodiscard]] std::uint32_t allocate_domain() noexcept;

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<renderer_state>> renderers_;
  struct surface_record {
    std::shared_ptr<renderer_state> renderer;
    VkSurfaceKHR native_handle{VK_NULL_HANDLE};
  };
  std::unordered_map<granit_surface, surface_record> surfaces_;
  std::uint32_t next_domain_{1};
};

} // namespace granit::detail

#endif
