// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_BINDING_H_
#define GRANIT_BACKEND_BINDING_H_

#include <cstdint>

#include "backend/resources.h"

namespace granit::detail {

enum class backend_binding_type : std::uint8_t {
  uniform_buffer,
  dynamic_uniform_buffer,
  storage_buffer,
  sampled_texture,
  sampled_texture_cube,
  storage_texture,
  sampler,
  comparison_sampler,
};

struct backend_bind_group_write {
  std::uint32_t binding{};
  std::uint32_t array_element{};
  backend_binding_type type{};
  backend_buffer_resource* buffer{};
  std::uint64_t offset{};
  std::uint64_t range{};
  backend_texture_view_resource* texture_view{};
  backend_sampler_resource* sampler{};
};

} // namespace granit::detail

#endif
