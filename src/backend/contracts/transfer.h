// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_TRANSFER_H_
#define GRANIT_BACKEND_TRANSFER_H_

#include <cstdint>
#include <span>

#include <granit/core/result.h>
#include <granit/renderer/command_recorder.h>
#include <granit/renderer/resource_types.h>

#include "backend/contracts/resources.h"

namespace granit::detail {

/** 统一命令记录器中的资源传输与填充能力。 */
class backend_transfer_command_renderer {
public:
  backend_transfer_command_renderer() = default;
  virtual ~backend_transfer_command_renderer() = default;
  backend_transfer_command_renderer(const backend_transfer_command_renderer&) = delete;
  backend_transfer_command_renderer& operator=(const backend_transfer_command_renderer&) = delete;

  [[nodiscard]] virtual granit_result
  copy_buffer(backend_command_recorder_resource& recorder, backend_buffer_resource& source,
              backend_buffer_resource& destination,
              std::span<const granit_buffer_copy_region> regions) = 0;
  [[nodiscard]] virtual granit_result
  copy_texture_to_buffer(backend_command_recorder_resource& recorder,
                         backend_texture_resource& source, backend_buffer_resource& destination,
                         granit_texture_format format, const granit_texture_data_layout& layout,
                         const granit_texture_write_region& region) = 0;
  [[nodiscard]] virtual granit_result
  copy_buffer_to_texture(backend_command_recorder_resource& recorder,
                         backend_buffer_resource& source, backend_texture_resource& destination,
                         granit_texture_format format, const granit_texture_data_layout& layout,
                         const granit_texture_write_region& region) = 0;
  [[nodiscard]] virtual granit_result copy_texture(backend_command_recorder_resource& recorder,
                                                   backend_texture_resource& source,
                                                   backend_texture_resource& destination,
                                                   const granit_texture_copy_region& region) = 0;
  [[nodiscard]] virtual bool
  texture_supports_linear_blit(granit_texture_format format) const noexcept = 0;
  [[nodiscard]] virtual granit_result
  generate_mipmaps(backend_command_recorder_resource& recorder, backend_texture_resource& texture,
                   const granit_texture_desc& desc, const granit_texture_mipmap_range& range) = 0;
  [[nodiscard]] virtual granit_result fill_buffer(backend_command_recorder_resource& recorder,
                                                  backend_buffer_resource& buffer,
                                                  std::uint64_t offset, std::uint64_t size,
                                                  std::uint32_t value) = 0;
};

} // namespace granit::detail

#endif
