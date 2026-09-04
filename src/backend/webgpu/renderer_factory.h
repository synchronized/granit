// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RENDERER_FACTORY_H_
#define GRANIT_BACKEND_WEBGPU_RENDERER_FACTORY_H_

#include "backend/plugin/plugin_api.h"

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>
#include <granit/renderer/renderer.h>

#include <cstdint>
#include <string_view>

namespace granit::detail {

[[nodiscard]] granit_result
create_webgpu_renderer_static(const granit_backend_plugin_api* api, std::uint32_t surface_types,
                              granit_diagnostic_callback diagnostic_callback,
                              void* diagnostic_user_data, granit_renderer& renderer);

[[nodiscard]] granit_result
create_webgpu_renderer_dynamic(std::string_view library_path, std::uint32_t surface_types,
                               granit_diagnostic_callback diagnostic_callback,
                               void* diagnostic_user_data, granit_renderer& renderer);

} // namespace granit::detail

#endif
