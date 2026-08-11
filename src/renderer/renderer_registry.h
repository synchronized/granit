// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <granit/renderer/buffer.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/pipeline.h>
#include <granit/renderer/renderer.h>
#include <granit/renderer/sampler.h>
#include <granit/renderer/shader.h>
#include <granit/renderer/surface.h>
#include <granit/renderer/swapchain.h>
#include <granit/renderer/texture.h>
#include <granit/renderer/upload_batch.h>

#include "core/handle_table.h"
#include "core/lifecycle_validation.h"
#include "renderer/renderer_state.h"

namespace granit::detail {

/** 线程安全地管理进程内公开 renderer 句柄。 */
class renderer_registry {
public:
  static renderer_registry& instance();

  [[nodiscard]] granit_result create(std::string_view application_name, bool enable_validation,
                                     std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                     granit_renderer& renderer);
  [[nodiscard]] granit_result destroy(granit_renderer renderer);
  [[nodiscard]] granit_result import_pipeline_cache(granit_renderer renderer, const void* data,
                                                    std::uint64_t size);
  [[nodiscard]] granit_result export_pipeline_cache(granit_renderer renderer, void* data,
                                                    std::uint64_t& size);
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
  [[nodiscard]] granit_result get_swapchain_backbuffer(granit_renderer renderer,
                                                       granit_swapchain swapchain,
                                                       std::uint32_t index, granit_texture& texture,
                                                       granit_texture_view& view);
  [[nodiscard]] granit_result
  acquire_swapchain_frame(granit_renderer renderer, granit_swapchain swapchain, granit_frame& frame,
                          std::uint32_t& image_index, bool& needs_recreate);
  [[nodiscard]] granit_result present_swapchain_frame(granit_renderer renderer,
                                                      granit_swapchain swapchain,
                                                      granit_frame frame, bool& needs_recreate);
  [[nodiscard]] granit_result cancel_swapchain_frame(granit_renderer renderer,
                                                     granit_swapchain swapchain, granit_frame frame,
                                                     bool& needs_recreate);
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
  [[nodiscard]] granit_result write_texture(granit_renderer renderer, granit_texture texture,
                                            const void* data, std::uint64_t size,
                                            const granit_texture_data_layout& layout,
                                            const granit_texture_write_region& region);
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
  [[nodiscard]] granit_result create_shader(granit_renderer renderer, granit_shader_stage stage,
                                            std::span<const std::uint32_t> code,
                                            std::string_view entry_point, granit_shader& shader);
  [[nodiscard]] granit_result destroy_shader(granit_renderer renderer, granit_shader shader);
  [[nodiscard]] granit_result
  create_bind_group_layout(granit_renderer renderer,
                           std::span<const granit_bind_group_layout_entry> entries,
                           granit_bind_group_layout& layout);
  [[nodiscard]] granit_result destroy_bind_group_layout(granit_renderer renderer,
                                                        granit_bind_group_layout layout);
  [[nodiscard]] granit_result create_bind_group(granit_renderer renderer,
                                                const granit_bind_group_desc& desc,
                                                granit_bind_group& bind_group);
  [[nodiscard]] granit_result destroy_bind_group(granit_renderer renderer,
                                                 granit_bind_group bind_group);
  [[nodiscard]] granit_result
  create_pipeline_layout(granit_renderer renderer,
                         std::span<const granit_bind_group_layout> bind_group_layouts,
                         granit_pipeline_layout& layout);
  [[nodiscard]] granit_result destroy_pipeline_layout(granit_renderer renderer,
                                                      granit_pipeline_layout layout);
  [[nodiscard]] granit_result create_graphics_pipeline(granit_renderer renderer,
                                                       const granit_graphics_pipeline_desc& desc,
                                                       granit_graphics_pipeline& pipeline);
  [[nodiscard]] granit_result destroy_graphics_pipeline(granit_renderer renderer,
                                                        granit_graphics_pipeline pipeline);
  [[nodiscard]] granit_result create_compute_pipeline(granit_renderer renderer,
                                                      const granit_compute_pipeline_desc& desc,
                                                      granit_compute_pipeline& pipeline);
  [[nodiscard]] granit_result destroy_compute_pipeline(granit_renderer renderer,
                                                       granit_compute_pipeline pipeline);
  [[nodiscard]] granit_result create_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder& recorder);
  [[nodiscard]] granit_result begin_command_recorder(granit_renderer renderer,
                                                     granit_command_recorder recorder);
  [[nodiscard]] granit_result end_command_recorder(granit_renderer renderer,
                                                   granit_command_recorder recorder);
  [[nodiscard]] granit_result submit_command_recorder(granit_renderer renderer,
                                                      granit_command_recorder recorder);
  [[nodiscard]] granit_result
  submit_command_recorders(granit_renderer renderer,
                           std::span<const granit_command_recorder> recorders);
  [[nodiscard]] granit_result submit_command_recorder_frame(granit_renderer renderer,
                                                            granit_command_recorder recorder,
                                                            granit_frame frame);
  [[nodiscard]] granit_result reset_command_recorder(granit_renderer renderer,
                                                     granit_command_recorder recorder);
  [[nodiscard]] granit_result copy_buffer(granit_renderer renderer,
                                          granit_command_recorder recorder, granit_buffer source,
                                          granit_buffer destination,
                                          std::span<const granit_buffer_copy_region> regions);
  [[nodiscard]] granit_result fill_buffer(granit_renderer renderer,
                                          granit_command_recorder recorder, granit_buffer buffer,
                                          std::uint64_t offset, std::uint64_t size,
                                          std::uint32_t value);
  [[nodiscard]] granit_result bind_graphics_pipeline(granit_renderer renderer,
                                                     granit_command_recorder recorder,
                                                     granit_graphics_pipeline pipeline);
  [[nodiscard]] granit_result bind_graphics_groups(granit_renderer renderer,
                                                   granit_command_recorder recorder,
                                                   granit_pipeline_layout layout,
                                                   std::uint32_t first_group,
                                                   std::span<const granit_bind_group> bind_groups);
  [[nodiscard]] granit_result bind_compute_pipeline(granit_renderer renderer,
                                                    granit_command_recorder recorder,
                                                    granit_compute_pipeline pipeline);
  [[nodiscard]] granit_result bind_compute_groups(granit_renderer renderer,
                                                  granit_command_recorder recorder,
                                                  granit_pipeline_layout layout,
                                                  std::uint32_t first_group,
                                                  std::span<const granit_bind_group> bind_groups);
  [[nodiscard]] granit_result dispatch(granit_renderer renderer, granit_command_recorder recorder,
                                       std::uint32_t group_count_x, std::uint32_t group_count_y,
                                       std::uint32_t group_count_z);
  [[nodiscard]] granit_result set_viewports(granit_renderer renderer,
                                            granit_command_recorder recorder, std::uint32_t first,
                                            std::span<const granit_viewport> viewports);
  [[nodiscard]] granit_result set_scissors(granit_renderer renderer,
                                           granit_command_recorder recorder, std::uint32_t first,
                                           std::span<const granit_scissor> scissors);
  [[nodiscard]] granit_result
  bind_vertex_buffers(granit_renderer renderer, granit_command_recorder recorder,
                      std::uint32_t first, std::span<const granit_vertex_buffer_binding> bindings);
  [[nodiscard]] granit_result bind_index_buffer(granit_renderer renderer,
                                                granit_command_recorder recorder,
                                                granit_buffer buffer, std::uint64_t offset,
                                                granit_index_type type);
  [[nodiscard]] granit_result draw(granit_renderer renderer, granit_command_recorder recorder,
                                   std::uint32_t vertex_count, std::uint32_t instance_count,
                                   std::uint32_t first_vertex, std::uint32_t first_instance);
  [[nodiscard]] granit_result draw_indexed(granit_renderer renderer,
                                           granit_command_recorder recorder,
                                           std::uint32_t index_count, std::uint32_t instance_count,
                                           std::uint32_t first_index, std::int32_t vertex_offset,
                                           std::uint32_t first_instance);
  [[nodiscard]] granit_result begin_rendering(granit_renderer renderer,
                                              granit_command_recorder recorder,
                                              const granit_rendering_desc& desc);
  [[nodiscard]] granit_result end_rendering(granit_renderer renderer,
                                            granit_command_recorder recorder);
  [[nodiscard]] granit_result destroy_command_recorder(granit_renderer renderer,
                                                       granit_command_recorder recorder);
  [[nodiscard]] granit_result create_upload_batch(granit_renderer renderer,
                                                  granit_upload_batch& batch);
  [[nodiscard]] granit_result upload_batch_write_buffer(granit_renderer renderer,
                                                        granit_upload_batch batch,
                                                        granit_buffer buffer, std::uint64_t offset,
                                                        const void* data, std::uint64_t size);
  [[nodiscard]] granit_result upload_batch_write_texture(granit_renderer renderer,
                                                         granit_upload_batch batch,
                                                         granit_texture texture, const void* data,
                                                         std::uint64_t size,
                                                         const granit_texture_data_layout& layout,
                                                         const granit_texture_write_region& region);
  [[nodiscard]] granit_result submit_upload_batch(granit_renderer renderer,
                                                  granit_upload_batch batch);
  [[nodiscard]] granit_result reset_upload_batch(granit_renderer renderer,
                                                 granit_upload_batch batch);
  [[nodiscard]] granit_result destroy_upload_batch(granit_renderer renderer,
                                                   granit_upload_batch batch);

private:
  renderer_registry() = default;

  struct swapchain_record;
  struct command_recorder_record;
  struct frame_record;
  struct upload_batch_record;

  struct resource_metadata {
    std::uint64_t creation_sequence{};
    std::atomic<submission_serial> last_use_serial{};
  };
  struct retained_resource {
    std::shared_ptr<void> resource;
    resource_metadata* metadata{};
  };

  [[nodiscard]] std::uint32_t allocate_domain() noexcept;
  [[nodiscard]] granit_result
  install_swapchain_backbuffers(granit_swapchain swapchain,
                                const std::shared_ptr<swapchain_record>& record);
  [[nodiscard]] std::shared_ptr<command_recorder_record>
  acquire_command_recorder(granit_renderer renderer, granit_command_recorder recorder);

  std::mutex mutex_;
  handle_table handles_;
  std::unordered_map<granit_renderer, std::shared_ptr<renderer_state>> renderers_;
  struct surface_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    VkSurfaceKHR native_handle{VK_NULL_HANDLE};
    ~surface_record();
  };
  struct swapchain_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<surface_record> surface;
    std::unique_ptr<vulkan_swapchain> native;
    std::vector<granit_texture> textures;
    std::vector<granit_texture_view> views;
    bool surface_lost{};
    ~swapchain_record();
  };
  struct buffer_record {
    resource_metadata metadata;
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
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    vulkan_image_allocation native;
    granit_texture_desc desc{};
    bool owned{true};
    bool publicly_destroyable{true};
    std::mutex mutex;
    ~texture_record();
  };
  struct texture_view_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<texture_record> texture;
    VkImageView native{VK_NULL_HANDLE};
    granit_texture_view_desc desc{};
    bool publicly_destroyable{true};
    ~texture_view_record();
  };
  struct sampler_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    VkSampler native{VK_NULL_HANDLE};
    ~sampler_record();
  };
  struct shader_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    VkShaderModule native{VK_NULL_HANDLE};
    granit_shader_stage stage{};
    std::string entry_point;
    ~shader_record();
  };
  struct bind_group_layout_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    VkDescriptorSetLayout native{VK_NULL_HANDLE};
    std::vector<granit_bind_group_layout_entry> entries;
    ~bind_group_layout_record();
  };
  struct pipeline_layout_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    VkPipelineLayout native{VK_NULL_HANDLE};
    std::vector<std::shared_ptr<bind_group_layout_record>> bind_group_layouts;
    ~pipeline_layout_record();
  };
  struct bind_group_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<bind_group_layout_record> layout;
    std::vector<std::shared_ptr<void>> resources;
    std::vector<std::pair<VkBuffer, VkAccessFlags2>> compute_buffer_accesses;
    std::vector<vulkan_image_access> compute_image_accesses;
    VkDescriptorPool pool{VK_NULL_HANDLE};
    VkDescriptorSet set{VK_NULL_HANDLE};
    ~bind_group_record();
  };
  struct graphics_pipeline_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> vertex_shader;
    std::shared_ptr<shader_record> fragment_shader;
    VkPipeline native{VK_NULL_HANDLE};
    ~graphics_pipeline_record();
  };
  struct compute_pipeline_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<pipeline_layout_record> layout;
    std::shared_ptr<shader_record> compute_shader;
    VkPipeline native{VK_NULL_HANDLE};
    ~compute_pipeline_record();
  };
  struct command_recorder_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    vulkan_command_recorder native;
    std::mutex mutex;
    std::vector<retained_resource> retained_resources;
    ~command_recorder_record();
  };
  struct frame_record {
    std::shared_ptr<renderer_state> renderer;
    std::shared_ptr<swapchain_record> swapchain;
    std::mutex mutex;
    std::uint32_t image_index{};
    std::size_t slot_index{};
    bool submitted{};
  };
  struct upload_entry {
    vulkan_upload_type type{vulkan_upload_type::buffer};
    std::shared_ptr<buffer_record> buffer;
    std::shared_ptr<texture_record> texture;
    std::uint64_t offset{};
    std::vector<std::byte> data;
    VkBufferImageCopy texture_copy{};
  };
  struct upload_batch_record {
    resource_metadata metadata;
    std::shared_ptr<renderer_state> renderer;
    std::mutex mutex;
    std::vector<upload_entry> uploads;
    bool failed{};
  };
  std::unordered_map<granit_surface, std::shared_ptr<surface_record>> surfaces_;
  std::unordered_map<granit_swapchain, std::shared_ptr<swapchain_record>> swapchains_;
  std::unordered_map<granit_buffer, std::shared_ptr<buffer_record>> buffers_;
  std::unordered_map<granit_texture, std::shared_ptr<texture_record>> textures_;
  std::unordered_map<granit_texture_view, std::shared_ptr<texture_view_record>> texture_views_;
  std::unordered_map<granit_sampler, std::shared_ptr<sampler_record>> samplers_;
  std::unordered_map<granit_shader, std::shared_ptr<shader_record>> shaders_;
  std::unordered_map<granit_pipeline_layout, std::shared_ptr<pipeline_layout_record>>
      pipeline_layouts_;
  std::unordered_map<granit_bind_group_layout, std::shared_ptr<bind_group_layout_record>>
      bind_group_layouts_;
  std::unordered_map<granit_bind_group, std::shared_ptr<bind_group_record>> bind_groups_;
  std::unordered_map<granit_graphics_pipeline, std::shared_ptr<graphics_pipeline_record>>
      graphics_pipelines_;
  std::unordered_map<granit_compute_pipeline, std::shared_ptr<compute_pipeline_record>>
      compute_pipelines_;
  std::unordered_map<granit_command_recorder, std::shared_ptr<command_recorder_record>>
      command_recorders_;
  std::unordered_map<granit_frame, std::shared_ptr<frame_record>> frames_;
  std::unordered_map<granit_upload_batch, std::shared_ptr<upload_batch_record>> upload_batches_;
  std::uint32_t next_domain_{1};
  std::uint64_t next_creation_sequence_{1};
};

} // namespace granit::detail

#endif
