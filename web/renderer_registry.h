// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WEB_RENDERER_REGISTRY_H_
#define GRANIT_WEB_RENDERER_REGISTRY_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

#include <granit/renderer/renderer.h>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>

#include "backend/presentation.h"
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
  [[nodiscard]] granit_result create_canvas_surface(granit_renderer renderer, const char* selector,
                                                    std::uint32_t selector_length,
                                                    granit_surface& surface);
  [[nodiscard]] granit_result destroy_surface(granit_renderer renderer, granit_surface surface);
  [[nodiscard]] granit_result create_swapchain(granit_renderer renderer, granit_surface surface,
                                               const backend_swapchain_desc& desc,
                                               granit_swapchain& swapchain);
  [[nodiscard]] granit_result recreate_swapchain(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 const backend_swapchain_desc& desc);
  [[nodiscard]] granit_result get_swapchain_info(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 backend_swapchain_info& info);
  [[nodiscard]] granit_result get_swapchain_backbuffer(granit_renderer renderer,
                                                       granit_swapchain swapchain,
                                                       std::uint32_t index, granit_texture& texture,
                                                       granit_texture_view& view);
  [[nodiscard]] granit_result acquire_swapchain(granit_renderer renderer,
                                                granit_swapchain swapchain, granit_frame& frame,
                                                std::uint32_t& image_index, bool& needs_recreate);
  [[nodiscard]] granit_result finish_frame(granit_renderer renderer, granit_swapchain swapchain,
                                           granit_frame frame, bool present, bool& needs_recreate);
  [[nodiscard]] granit_result get_frame_info(granit_renderer renderer, granit_swapchain swapchain,
                                             granit_frame frame, std::uint32_t& frame_slot,
                                             std::uint32_t& frame_slot_count);
  [[nodiscard]] granit_result destroy_swapchain(granit_renderer renderer,
                                                granit_swapchain swapchain);

private:
  struct surface_record {
    std::shared_ptr<webgpu_renderer_state> renderer;
    std::unique_ptr<backend_surface_resource> native;
  };
  struct swapchain_record {
    std::shared_ptr<webgpu_renderer_state> renderer;
    std::shared_ptr<surface_record> surface;
    std::unique_ptr<backend_swapchain_resource> native;
    granit_texture texture{};
    granit_texture_view view{};
    std::uint32_t image_index{};
  };
  struct frame_record {
    std::shared_ptr<swapchain_record> swapchain;
    backend_acquired_swapchain_frame acquired;
  };

  [[nodiscard]] std::shared_ptr<webgpu_renderer_state> acquire(granit_renderer renderer);
  void erase_backbuffer(swapchain_record& swapchain) noexcept;

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<webgpu_renderer_state>> renderers_;
  std::unordered_map<granit_surface, std::shared_ptr<surface_record>> surfaces_;
  std::unordered_map<granit_swapchain, std::shared_ptr<swapchain_record>> swapchains_;
  std::unordered_map<granit_frame, std::shared_ptr<frame_record>> frames_;
};

} // namespace granit::detail

#endif
