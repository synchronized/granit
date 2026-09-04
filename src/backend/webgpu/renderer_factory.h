// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_RENDERER_FACTORY_H_
#define GRANIT_BACKEND_WEBGPU_RENDERER_FACTORY_H_

#include "backend/webgpu/provider_api.h"

#include <granit/core/diagnostic.h>
#include <granit/core/result.h>
#include <granit/renderer/renderer.h>

#include <cstdint>

namespace granit::detail {

[[nodiscard]] granit_result
create_webgpu_renderer_static(const granit_webgpu_provider_api* api, std::uint32_t surface_types,
                              granit_diagnostic_callback diagnostic_callback,
                              void* diagnostic_user_data, granit_renderer& renderer);

} // namespace granit::detail

#endif
