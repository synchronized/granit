// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/granit.hpp>

#include "linkage_check.h"

#include <array>
#include <atomic>
#include <string>
#include <string_view>
#include <utility>

namespace {

struct diagnostic_capture {
  std::atomic<bool> found_buffer_desc{};
};

void capture_diagnostic(granit_diagnostic_severity, granit_diagnostic_category category,
                        const char* message, std::uint32_t message_length,
                        void* user_data) noexcept {
  if (category != GRANIT_DIAGNOSTIC_CATEGORY_VALIDATION)
    return;
  const std::string_view text{message, message_length};
  if (text.find("granit_buffer_create") != std::string_view::npos &&
      text.find("desc") != std::string_view::npos) {
    static_cast<diagnostic_capture*>(user_data)->found_buffer_desc.store(true);
  }
}

} // namespace

int main() {
  const auto runtime = granit::library_version();
  const auto header = std::to_string(GRANIT_VERSION_MAJOR) + "." +
                      std::to_string(GRANIT_VERSION_MINOR) + "." +
                      std::to_string(GRANIT_VERSION_PATCH);
  if (header != GRANIT_CONSUMER_PACKAGE_VERSION)
    return 1;
  if (runtime.major != GRANIT_VERSION_MAJOR || runtime.minor != GRANIT_VERSION_MINOR ||
      runtime.patch != GRANIT_VERSION_PATCH)
    return 2;
  if (!granit::result::success || granit::result::invalid_argument ||
      !granit::result::invalid_argument.failed() ||
      granit::result::not_ready.native() != GRANIT_ERROR_NOT_READY ||
      granit::result::invalid_handle.message() != "invalid handle")
    return 12;

  diagnostic_capture diagnostics;
  granit::renderer renderer;
  const auto renderer_result = renderer.initialize(
      {.diagnostics = capture_diagnostic, .diagnostic_user_data = &diagnostics});
  if (renderer_result == granit::result::backend_unavailable ||
      renderer_result == granit::result::incompatible_driver ||
      renderer_result == granit::result::no_suitable_device)
    return renderer.valid() ? 3 : 0;
  if (renderer_result.failed())
    return 4;

  granit::renderer_limits limits;
  if ((renderer.get_limits(limits)).failed() || limits.uniform_buffer_offset_alignment == 0 ||
      limits.max_uniform_buffer_binding_size == 0)
    return 5;
  granit::renderer_shader_capabilities shader_capabilities;
  if ((renderer.get_shader_capabilities(shader_capabilities)).failed() ||
      shader_capabilities.backend != granit::renderer_backend::vulkan ||
      shader_capabilities.profile != GRANIT_SHADER_PROFILE_PORTABLE)
    return 11;
  const std::array shader_variants{
      granit::shader_variant_requirement{.backend = granit::renderer_backend::vulkan}};
  const auto [variant_result, variant_index] = renderer.select_shader_variant(shader_variants);
  if (variant_result.failed() || variant_index != 0)
    return 13;

  granit_buffer_desc invalid_desc = GRANIT_BUFFER_DESC_INIT;
  granit_buffer invalid_buffer = GRANIT_NULL_HANDLE;
  if (granit_buffer_create(renderer.native_handle(), &invalid_desc, &invalid_buffer) !=
          GRANIT_ERROR_INVALID_ARGUMENT ||
      invalid_buffer != GRANIT_NULL_HANDLE || !diagnostics.found_buffer_desc.load())
    return 10;

  granit::buffer buffer;
  if ((buffer.initialize(renderer.native_handle(), {.size = 64,
                                                    .usage = granit::buffer_usage::transfer_source,
                                                    .location = granit::memory_location::upload}))
          .failed())
    return 6;
  granit::buffer moved = std::move(buffer);
  if (buffer.valid() || !moved.valid())
    return 7;
  if ((moved.reset()).failed() || (moved.reset()).failed())
    return 8;
  granit::renderer moved_renderer = std::move(renderer);
  if (renderer.valid() || !moved_renderer.valid())
    return 9;
  if ((moved_renderer.reset()).failed() || (moved_renderer.reset()).failed())
    return 11;
  return 0;
}
