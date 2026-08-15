// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <array>
#include <string>

#include <catch2/catch_all.hpp>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr std::array expected_symbols{
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
    "granit_result_message",
    "granit_sampler_create",
    "granit_sampler_destroy",
    "granit_shader_create",
    "granit_shader_destroy",
    "granit_surface_create_win32",
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

TEST_CASE("共享库导出完整的公共 C ABI", "[abi][exports]") {
  const shared_library library{GRANIT_ABI_LIBRARY_PATH};
  REQUIRE(library.is_open());

  for (const char* symbol : expected_symbols) {
    INFO("缺少公共导出符号: " << symbol);
    CHECK(library.contains(symbol));
  }
}
