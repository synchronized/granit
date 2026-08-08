// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>

#include <granit/buffer.h>
#include <granit/renderer.h>
#include <granit/sampler.h>
#include <granit/surface.h>
#include <granit/swapchain.h>
#include <granit/texture.h>

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
  [[nodiscard]] granit_result create_swapchain(granit_renderer renderer, granit_surface surface,
                                               const vulkan_swapchain_desc& desc,
                                               granit_swapchain& swapchain);
  [[nodiscard]] granit_result recreate_swapchain(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 const vulkan_swapchain_desc& desc);
  [[nodiscard]] granit_result get_swapchain_info(granit_renderer renderer,
                                                 granit_swapchain swapchain,
                                                 vulkan_swapchain_info& info);
  [[nodiscard]] granit_result destroy_swapchain(granit_renderer renderer,
                                                granit_swapchain swapchain);
  [[nodiscard]] granit_result create_buffer(granit_renderer renderer,
                                            const granit_buffer_desc& desc, granit_buffer& buffer);
  [[nodiscard]] granit_result map_buffer(granit_renderer renderer, granit_buffer buffer,
                                         std::uint64_t offset, std::uint64_t size, void*& data);
  [[nodiscard]] granit_result unmap_buffer(granit_renderer renderer, granit_buffer buffer);
  [[nodiscard]] granit_result destroy_buffer(granit_renderer renderer, granit_buffer buffer);
  [[nodiscard]] granit_result write_buffer(granit_renderer renderer, granit_buffer buffer,
                                           std::uint64_t offset, const void* data,
                                           std::uint64_t size);
  [[nodiscard]] granit_result create_texture(granit_renderer renderer,
                                             const granit_texture_desc& desc,
                                             granit_texture& texture);
  [[nodiscard]] granit_result create_texture_view(granit_renderer renderer, granit_texture texture,
                                                  const granit_texture_view_desc& desc,
                                                  granit_texture_view& view);
  [[nodiscard]] granit_result destroy_texture_view(granit_renderer renderer,
                                                   granit_texture_view view);
  [[nodiscard]] granit_result destroy_texture(granit_renderer renderer, granit_texture texture);
  [[nodiscard]] granit_result create_sampler(granit_renderer renderer,
                                             const granit_sampler_desc& desc,
                                             granit_sampler& sampler);
  [[nodiscard]] granit_result destroy_sampler(granit_renderer renderer, granit_sampler sampler);

private:
  renderer_registry() = default;

  [[nodiscard]] std::uint32_t allocate_domain() noexcept;

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<renderer_state>> renderers_;
  struct surface_record {
    std::shared_ptr<renderer_state> renderer;
    VkSurfaceKHR native_handle{VK_NULL_HANDLE};
    ~surface_record();
  };
  struct swapchain_record {
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<surface_record> surface;
    std::unique_ptr<vulkan_swapchain> native;
    ~swapchain_record();
  };
  struct buffer_record {
    std::shared_ptr<renderer_state> renderer;
    vulkan_buffer_allocation native;
    granit_buffer_desc desc{};
    std::mutex mutex;
    bool mapped{};
    std::uint64_t mapped_offset{};
    std::uint64_t mapped_size{};
    ~buffer_record();
  };
  struct texture_record {
    std::shared_ptr<renderer_state> renderer;
    vulkan_image_allocation native;
    granit_texture_desc desc{};
    bool owned{true};
    ~texture_record();
  };
  struct texture_view_record {
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<texture_record> texture;
    VkImageView native{VK_NULL_HANDLE};
    ~texture_view_record();
  };
  struct sampler_record {
    std::shared_ptr<renderer_state> renderer;
    VkSampler native{VK_NULL_HANDLE};
    ~sampler_record();
  };
  std::unordered_map<granit_surface, std::shared_ptr<surface_record>> surfaces_;
  std::unordered_map<granit_swapchain, std::shared_ptr<swapchain_record>> swapchains_;
  std::unordered_map<granit_buffer, std::shared_ptr<buffer_record>> buffers_;
  std::unordered_map<granit_texture, std::shared_ptr<texture_record>> textures_;
  std::unordered_map<granit_texture_view, std::shared_ptr<texture_view_record>> texture_views_;
  std::unordered_map<granit_sampler, std::shared_ptr<sampler_record>> samplers_;
  std::uint32_t next_domain_{1};
};

} // namespace granit::detail

#endif
