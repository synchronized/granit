// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_CORE_RESOURCE_VALIDATION_H_
#define GRANIT_CORE_RESOURCE_VALIDATION_H_

#include <granit/resource_types.h>
#include <granit/result.h>

namespace granit::detail {

[[nodiscard]] granit_result validate_buffer_desc(const granit_buffer_desc& desc) noexcept;
[[nodiscard]] granit_result validate_texture_desc(const granit_texture_desc& desc) noexcept;
[[nodiscard]] granit_result
validate_texture_view_desc(const granit_texture_view_desc& desc) noexcept;
[[nodiscard]] granit_result validate_sampler_desc(const granit_sampler_desc& desc) noexcept;

} // namespace granit::detail

#endif
