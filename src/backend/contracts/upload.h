// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_UPLOAD_H_
#define GRANIT_BACKEND_UPLOAD_H_

#include <cstdint>

#include <granit/renderer/resource_types.h>

#include "backend/contracts/resources.h"

namespace granit::detail {

enum class backend_upload_type { buffer, texture };

struct backend_texture_copy {
  std::uint32_t buffer_row_length{};
  std::uint32_t buffer_image_height{};
  granit_texture_aspect aspect{};
  std::uint32_t mip_level{};
  std::uint32_t base_array_layer{};
  std::uint32_t array_layer_count{};
  std::int32_t x{};
  std::int32_t y{};
  std::int32_t z{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t depth{};
};

struct backend_upload_operation {
  backend_upload_type type{backend_upload_type::buffer};
  const backend_buffer_resource* buffer{};
  const backend_texture_resource* texture{};
  std::uint64_t destination_offset{};
  const void* data{};
  std::uint64_t size{};
  backend_texture_copy texture_copy{};
};

} // namespace granit::detail

#endif
