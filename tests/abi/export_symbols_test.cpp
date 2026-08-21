// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <span>
#include <string>

#include <catch2/catch_all.hpp>

#include "snapshots/0.1.0/core_identity.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr std::array core_symbols{
    "granit_bind_group_create",
    "granit_bind_group_destroy",
    "granit_bind_group_layout_create",
    "granit_bind_group_layout_destroy",
    "granit_buffer_create",
    "granit_buffer_create_with_data",
    "granit_buffer_destroy",
    "granit_buffer_get_desc",
    "granit_buffer_map",
    "granit_buffer_unmap",
    "granit_buffer_write",
    "granit_command_recorder_begin",
    "granit_command_recorder_begin_rendering",
    "granit_command_recorder_bind_compute_groups",
    "granit_command_recorder_bind_compute_pipeline",
    "granit_command_recorder_bind_graphics_groups",
    "granit_command_recorder_bind_graphics_pipeline",
    "granit_command_recorder_bind_index_buffer",
    "granit_command_recorder_bind_vertex_buffers",
    "granit_command_recorder_copy_buffer",
    "granit_command_recorder_copy_buffer_to_texture",
    "granit_command_recorder_copy_texture",
    "granit_command_recorder_copy_texture_to_buffer",
    "granit_command_recorder_create",
    "granit_command_recorder_destroy",
    "granit_command_recorder_dispatch",
    "granit_command_recorder_draw",
    "granit_command_recorder_draw_indexed",
    "granit_command_recorder_end",
    "granit_command_recorder_end_rendering",
    "granit_command_recorder_fill_buffer",
    "granit_command_recorder_generate_mipmaps",
    "granit_command_recorder_reset",
    "granit_command_recorder_reset_timestamp_queries",
    "granit_command_recorder_set_scissors",
    "granit_command_recorder_set_viewports",
    "granit_command_recorder_submit",
    "granit_command_recorder_submit_batch",
    "granit_command_recorder_submit_frame",
    "granit_command_recorder_write_timestamp",
    "granit_compute_pipeline_create",
    "granit_compute_pipeline_destroy",
    "granit_frame_cancel",
    "granit_graphics_pipeline_create",
    "granit_graphics_pipeline_destroy",
    "granit_pipeline_layout_create",
    "granit_pipeline_layout_destroy",
    "granit_renderer_create",
    "granit_renderer_destroy",
    "granit_renderer_pipeline_cache_export",
    "granit_renderer_pipeline_cache_import",
    "granit_renderer_set_object_name",
    "granit_result_message",
    "granit_sampler_create",
    "granit_sampler_destroy",
    "granit_shader_create",
    "granit_shader_destroy",
    "granit_surface_create_win32",
    "granit_surface_create_xcb",
    "granit_surface_create_wayland",
    "granit_surface_destroy",
    "granit_swapchain_acquire",
    "granit_swapchain_create",
    "granit_swapchain_destroy",
    "granit_swapchain_get_backbuffer",
    "granit_swapchain_get_info",
    "granit_swapchain_present",
    "granit_swapchain_recreate",
    "granit_texture_create",
    "granit_texture_create_with_default_view",
    "granit_texture_destroy",
    "granit_texture_format_get_footprint",
    "granit_texture_read",
    "granit_texture_view_create",
    "granit_texture_view_destroy",
    "granit_texture_write",
    "granit_timestamp_query_pool_create",
    "granit_timestamp_query_pool_destroy",
    "granit_timestamp_query_pool_get_results",
    "granit_upload_batch_create",
    "granit_upload_batch_destroy",
    "granit_upload_batch_reset",
    "granit_upload_batch_submit",
    "granit_upload_batch_write_buffer",
    "granit_upload_batch_write_texture",
    "granit_version_major",
    "granit_version_minor",
    "granit_version_patch",
};

constexpr std::array render_pipeline_symbols{
    "granit_canvas_draw_list_append",
    "granit_canvas_draw_list_append_rect",
    "granit_canvas_draw_list_clear",
    "granit_canvas_draw_list_create",
    "granit_canvas_draw_list_destroy",
    "granit_canvas_draw_list_get_stats",
    "granit_canvas_draw_list_record",
    "granit_debug_draw_list_append_lines",
    "granit_debug_draw_list_append_screen_to_canvas",
    "granit_debug_draw_list_append_triangles",
    "granit_debug_draw_list_clear",
    "granit_debug_draw_list_create",
    "granit_debug_draw_list_destroy",
    "granit_debug_draw_list_get_stats",
    "granit_debug_draw_list_record_world",
    "granit_material_create",
    "granit_material_destroy",
    "granit_material_parameter_id",
    "granit_material_update",
    "granit_mesh_create",
    "granit_mesh_destroy",
    "granit_render_pipeline_create",
    "granit_render_pipeline_destroy",
    "granit_render_pipeline_render",
    "granit_scene_snapshot_create",
    "granit_scene_snapshot_destroy",
    "granit_text_atlas_create",
    "granit_text_atlas_destroy",
    "granit_text_atlas_get_stats",
    "granit_text_atlas_upload_glyph",
    "granit_text_draw_list_append_glyph_run",
    "granit_text_draw_list_append_to_canvas",
    "granit_text_draw_list_clear",
    "granit_text_draw_list_create",
    "granit_text_draw_list_destroy",
    "granit_text_draw_list_get_stats",
};

constexpr std::array window_symbols{
    "granit_window_create",        "granit_window_destroy",        "granit_window_get_wayland",
    "granit_window_get_win32",     "granit_window_get_xcb",        "granit_window_poll_event",
    "granit_window_system_create", "granit_window_system_destroy",
};

constexpr std::array input_symbols{
    "granit_input_get_keyboard_state", "granit_input_get_pointer_state", "granit_input_poll_event",
    "granit_input_system_create",      "granit_input_system_destroy",
};

class shared_library {
public:
  explicit shared_library(const char* path) {
#if defined(_WIN32)
    handle_ = LoadLibraryA(path);
#else
    handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
  }

  ~shared_library() {
    if (handle_ == nullptr) {
      return;
    }
#if defined(_WIN32)
    FreeLibrary(handle_);
#else
    dlclose(handle_);
#endif
  }

  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;

  [[nodiscard]] bool is_open() const { return handle_ != nullptr; }

  [[nodiscard]] bool contains(const char* symbol) const {
#if defined(_WIN32)
    return GetProcAddress(handle_, symbol) != nullptr;
#else
    return dlsym(handle_, symbol) != nullptr;
#endif
  }

private:
#if defined(_WIN32)
  HMODULE handle_{nullptr};
#else
  void* handle_{nullptr};
#endif
};

} // namespace

void check_exports(const char* component, const char* path, std::span<const char* const> symbols) {
  INFO("component: " << component);
  const shared_library library{path};
  REQUIRE(library.is_open());

  for (const char* symbol : symbols) {
    INFO("缺少公共导出符号: " << symbol);
    CHECK(library.contains(symbol));
  }
}

TEST_CASE("共享库导出完整的公共 C ABI", "[abi][exports]") {
  INFO("ABI 快照: " << GRANIT_ABI_SNAPSHOT_COMPONENT << " " << GRANIT_ABI_SNAPSHOT_VERSION);
  check_exports("Core", GRANIT_ABI_CORE_LIBRARY_PATH, core_symbols);
  check_exports("RenderPipeline", GRANIT_ABI_RENDER_PIPELINE_LIBRARY_PATH, render_pipeline_symbols);
  check_exports("Window", GRANIT_ABI_WINDOW_LIBRARY_PATH, window_symbols);
  check_exports("Input", GRANIT_ABI_INPUT_LIBRARY_PATH, input_symbols);
}
