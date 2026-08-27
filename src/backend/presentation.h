// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PRESENTATION_H_
#define GRANIT_BACKEND_PRESENTATION_H_

#include <cstdint>
#include <memory>

#include <granit/renderer/resource_types.h>

#include "backend/resources.h"

namespace granit::detail {

struct backend_swapchain_desc {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t minimum_image_count{};
  std::uint32_t present_mode{};
};

struct backend_swapchain_info {
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t image_count{};
  std::uint32_t present_mode{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
};

struct backend_swapchain_backbuffer {
  std::unique_ptr<backend_texture_resource> texture;
  granit_texture_desc desc{};
};

} // namespace granit::detail

#endif
