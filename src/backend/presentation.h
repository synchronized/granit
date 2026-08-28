// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PRESENTATION_H_
#define GRANIT_BACKEND_PRESENTATION_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include <granit/renderer/resource_types.h>

#include "backend/resources.h"

namespace granit::detail {

struct backend_swapchain_desc {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t minimum_image_count{};
  std::uint32_t present_mode{};
};

struct backend_swapchain_info {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t image_count{};
  std::uint32_t present_mode{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
};

struct backend_swapchain_backbuffer {
  std::unique_ptr<backend_texture_resource> texture;
  std::unique_ptr<backend_texture_view_resource> view;
  granit_texture_desc desc{};
};

/** 后端 Acquire 结果；dynamic_backbuffer 仅在按帧获取纹理的后端中非空。 */
struct backend_acquired_swapchain_frame {
  std::uint32_t image_index{};
  std::size_t slot_index{};
  bool needs_recreate{};
  backend_swapchain_backbuffer dynamic_backbuffer;
};

/** Surface 与 Swapchain 的后端无关契约。 */
class backend_presentation_renderer {
public:
  backend_presentation_renderer() = default;
  virtual ~backend_presentation_renderer() = default;

  backend_presentation_renderer(const backend_presentation_renderer&) = delete;
  backend_presentation_renderer& operator=(const backend_presentation_renderer&) = delete;

  [[nodiscard]] virtual std::unique_ptr<backend_surface_resource> allocate_surface_resource() = 0;
  [[nodiscard]] virtual std::unique_ptr<backend_swapchain_resource>
  allocate_swapchain_resource() = 0;
  [[nodiscard]] virtual granit_result
  create_win32_surface(void* native_instance, void* native_window,
                       backend_surface_resource& surface) noexcept = 0;
  [[nodiscard]] virtual granit_result
  create_xcb_surface(void* connection, std::uint32_t window,
                     backend_surface_resource& surface) noexcept = 0;
  [[nodiscard]] virtual granit_result
  create_wayland_surface(void* display, void* native_surface,
                         backend_surface_resource& surface) noexcept = 0;
  [[nodiscard]] virtual granit_result
  create_canvas_surface(std::string_view selector, backend_surface_resource& surface) noexcept = 0;
  [[nodiscard]] virtual granit_result create_swapchain(backend_surface_resource& surface,
                                                       const backend_swapchain_desc& desc,
                                                       backend_swapchain_resource& swapchain) = 0;
  [[nodiscard]] virtual granit_result recreate_swapchain(backend_surface_resource& surface,
                                                         const backend_swapchain_desc& desc,
                                                         backend_swapchain_resource& swapchain) = 0;
  [[nodiscard]] virtual backend_swapchain_info
  get_swapchain_info(backend_swapchain_resource& swapchain) noexcept = 0;
  [[nodiscard]] virtual granit_result
  get_swapchain_backbuffers(backend_swapchain_resource& swapchain,
                            std::vector<backend_swapchain_backbuffer>& backbuffers) = 0;
  [[nodiscard]] virtual granit_result
  acquire_swapchain_frame(backend_swapchain_resource& swapchain,
                          backend_acquired_swapchain_frame& frame) = 0;
  [[nodiscard]] virtual granit_result present_swapchain_frame(backend_swapchain_resource& swapchain,
                                                              std::uint32_t image_index,
                                                              std::size_t slot_index,
                                                              bool& needs_recreate) = 0;
  [[nodiscard]] virtual granit_result cancel_swapchain_frame(backend_swapchain_resource& swapchain,
                                                             std::uint32_t image_index,
                                                             std::size_t slot_index,
                                                             bool& needs_recreate) = 0;
};

} // namespace granit::detail

#endif
