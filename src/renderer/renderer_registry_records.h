// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RENDERER_RENDERER_REGISTRY_RECORDS_H_
#define GRANIT_RENDERER_RENDERER_REGISTRY_RECORDS_H_

#include "renderer/renderer_registry.h"

namespace granit::detail {

struct renderer_registry::resource_metadata {
  std::uint64_t creation_sequence{};
  std::atomic<submission_serial> last_use_serial{};
};
struct renderer_registry::retained_resource {
  std::shared_ptr<void> resource;
  resource_metadata* metadata{};
};
struct renderer_registry::surface_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_presentation_renderer> renderer;
  std::unique_ptr<backend_surface_resource> native;
};
struct renderer_registry::swapchain_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_presentation_renderer> presentation;
  std::shared_ptr<surface_record> surface;
  std::unique_ptr<backend_swapchain_resource> native;
  std::vector<granit_texture> textures;
  std::vector<granit_texture_view> views;
  bool surface_lost{};
};
struct renderer_registry::buffer_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_buffer_resource> native;
  granit_buffer_desc desc{};
  std::mutex mutex;
  bool mapped{};
  std::uint64_t mapped_offset{};
  std::uint64_t mapped_size{};
};
struct renderer_registry::texture_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_texture_resource> native;
  granit_texture_desc desc{};
  bool publicly_destroyable{true};
  std::mutex mutex;
};
struct renderer_registry::texture_view_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<texture_record> texture;
  std::unique_ptr<backend_texture_view_resource> native;
  granit_texture_view_desc desc{};
  bool publicly_destroyable{true};
};
struct renderer_registry::sampler_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_sampler_resource> native;
};
struct renderer_registry::shader_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_shader_resource> native;
  granit_shader_stage stage{};
  std::string entry_point;
};
struct renderer_registry::bind_group_layout_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_bind_group_layout_resource> native;
  std::vector<granit_bind_group_layout_entry> entries;
};
struct renderer_registry::pipeline_layout_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_pipeline_layout_renderer> pipelines;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_pipeline_layout_resource> native;
  std::vector<std::shared_ptr<bind_group_layout_record>> bind_group_layouts;
};
struct renderer_registry::bind_group_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<bind_group_layout_record> layout;
  std::vector<std::shared_ptr<void>> resources;
  std::vector<backend_buffer_access> graphics_buffer_accesses;
  std::vector<backend_texture_access> graphics_texture_accesses;
  std::vector<backend_buffer_access> compute_buffer_accesses;
  std::vector<backend_texture_access> compute_texture_accesses;
  std::vector<dynamic_uniform_binding> dynamic_uniform_bindings;
  std::unique_ptr<backend_bind_group_resource> native;
};
struct renderer_registry::graphics_pipeline_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_pipeline_renderer> pipelines;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<pipeline_layout_record> layout;
  std::shared_ptr<shader_record> vertex_shader;
  std::shared_ptr<shader_record> fragment_shader;
  std::unique_ptr<backend_graphics_pipeline_resource> native;
};
struct renderer_registry::compute_pipeline_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<pipeline_layout_record> layout;
  std::shared_ptr<shader_record> compute_shader;
  std::unique_ptr<backend_compute_pipeline_resource> native;
};
struct renderer_registry::command_recorder_record {
  enum class web_state { initial, recording, rendering, executable, submitted };
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_queue> queue;
  std::shared_ptr<backend_command_renderer> commands;
  std::shared_ptr<backend_compute_command_renderer> compute;
  std::shared_ptr<backend_graphics_command_renderer> graphics;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::shared_ptr<backend_timestamp_renderer> timestamps;
  std::shared_ptr<backend_transfer_command_renderer> transfers;
  std::unique_ptr<backend_command_recorder_resource> native;
  std::mutex mutex;
  std::vector<retained_resource> retained_resources;
  std::shared_ptr<texture_view_record> web_target;
  std::shared_ptr<graphics_pipeline_record> web_pipeline;
  web_state web_status{web_state::initial};
  bool web_drew{};
  bool platform_managed_rendering{};
  bool owned_by_frame_context{};
};
enum class renderer_registry::frame_context_slot_state { idle, recording, submitted };
struct renderer_registry::frame_context_slot {
  granit_command_recorder recorder{GRANIT_NULL_HANDLE};
  granit_frame frame{GRANIT_NULL_HANDLE};
  frame_context_slot_state state{frame_context_slot_state::idle};
};
struct renderer_registry::frame_context_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::mutex mutex;
  std::vector<frame_context_slot> slots;
};
struct renderer_registry::timestamp_query_pool_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_timestamp_renderer> timestamps;
  std::shared_ptr<backend_retirement_renderer> retirement;
  std::unique_ptr<backend_timestamp_query_pool_resource> native;
  std::mutex mutex;
};
struct renderer_registry::frame_record {
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_presentation_renderer> presentation;
  std::shared_ptr<backend_queue> queue;
  std::shared_ptr<swapchain_record> swapchain;
  std::mutex mutex;
  std::uint32_t image_index{};
  std::size_t slot_index{};
  bool submitted{};
  bool dynamic_backbuffer{};
};
struct renderer_registry::upload_entry {
  backend_upload_type type{backend_upload_type::buffer};
  std::shared_ptr<buffer_record> buffer;
  std::shared_ptr<texture_record> texture;
  std::uint64_t offset{};
  std::vector<std::byte> data;
  backend_texture_copy texture_copy{};
};
struct renderer_registry::upload_batch_record {
  resource_metadata metadata;
  std::shared_ptr<backend_renderer> owner;
  std::shared_ptr<backend_resource_renderer> resource_api;
  std::mutex mutex;
  std::vector<upload_entry> uploads;
  bool failed{};
};

} // namespace granit::detail

#endif
