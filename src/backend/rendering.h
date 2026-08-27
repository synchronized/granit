// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_RENDERING_H_
#define GRANIT_BACKEND_RENDERING_H_

#include <granit/renderer/render_target.h>
#include <granit/renderer/resource_types.h>

#include "backend/resources.h"

namespace granit::detail {

struct backend_color_attachment {
  backend_texture_resource* texture{};
  backend_texture_view_resource* view{};
  granit_subresource_range range{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  granit_attachment_load_operation load_operation{};
  granit_attachment_store_operation store_operation{};
  granit_clear_color_value clear_value{};
};

struct backend_depth_stencil_attachment {
  backend_texture_resource* texture{};
  backend_texture_view_resource* view{};
  granit_subresource_range range{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  granit_attachment_load_operation depth_load_operation{};
  granit_attachment_store_operation depth_store_operation{};
  granit_attachment_load_operation stencil_load_operation{};
  granit_attachment_store_operation stencil_store_operation{};
  granit_clear_depth_stencil_value clear_value{};
};

} // namespace granit::detail

#endif
