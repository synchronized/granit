// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_factory.h"
#include "renderer/renderer_registry.h"
#include "core/diagnostic_sink.h"

#include "backend/vulkan/vulkan_renderer_state.h"

#include <new>
#include <string>
#include <string_view>

namespace granit::detail {

namespace {

granit_result create_vulkan_renderer(std::string_view application_name, bool enable_validation,
                                        std::uint32_t surface_types, std::uint32_t frames_in_flight,
                                        granit_diagnostic_callback diagnostic_callback,
                                        void* diagnostic_user_data, granit_renderer& renderer) {
  try {
    auto state = std::make_shared<vulkan_renderer_state>();
    const auto initialize_result =
        state->initialize(application_name, enable_validation, surface_types, frames_in_flight,
                          diagnostic_callback, diagnostic_user_data);
    if (initialize_result != GRANIT_SUCCESS)
      return initialize_result;

    return renderer_registry::instance().register_backend(std::move(state), renderer);
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void emit_backend_attempt(const diagnostic_sink& diagnostics, std::string_view backend,
                          granit_result result) {
  try {
    std::string message{"Renderer 尝试后端 "};
    message.append(backend);
    message.append(" 失败：");
    message.append(granit_result_message(result));
    diagnostics.emit(diagnostic_severity::warning, diagnostic_category::device, message);
  } catch (...) {
  }
}

} // namespace

granit_result create_default_renderer(const granit_renderer_desc& desc, granit_renderer& renderer) {
  constexpr std::string_view default_application_name = "Granit Application";
  const auto application_name =
      desc.application_name == nullptr
          ? default_application_name
          : std::string_view{desc.application_name, desc.application_name_length};
  const auto validation_enabled = (desc.flags & GRANIT_RENDERER_ENABLE_VALIDATION_BIT) != 0;
  const auto surface_types = desc.surface_types;
  const auto frames_in_flight = desc.frames_in_flight;
  const auto diagnostic_callback = desc.diagnostic_callback;
  auto* diagnostic_user_data = desc.diagnostic_user_data;
  const auto backend = desc.backend;
  const diagnostic_sink diagnostics{diagnostic_callback, diagnostic_user_data};
  if (backend == GRANIT_RENDERER_BACKEND_WEBGPU) {
    emit_backend_attempt(diagnostics, "WebGPU", GRANIT_ERROR_BACKEND_UNAVAILABLE);
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }

 granit_result vulkan_result =
      create_vulkan_renderer(application_name, validation_enabled, surface_types, frames_in_flight,
                      diagnostic_callback, diagnostic_user_data, renderer);
  return vulkan_result;
}

} // namespace granit::detail
