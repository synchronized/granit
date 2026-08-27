// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_ACCESS_H_
#define GRANIT_BACKEND_ACCESS_H_

#include <granit/renderer/resource_types.h>

#include "backend/resources.h"

namespace granit::detail {

enum class backend_buffer_access_type { uniform_read, storage_read_write };
enum class backend_texture_access_type { sampled_read, storage_read_write };

struct backend_buffer_access {
  backend_buffer_resource* buffer{};
  backend_buffer_access_type type{backend_buffer_access_type::uniform_read};
};

struct backend_texture_access {
  backend_texture_resource* texture{};
  granit_subresource_range range{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  backend_texture_access_type type{backend_texture_access_type::sampled_read};
};

} // namespace granit::detail

#endif
