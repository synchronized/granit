// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_READBACK_H_
#define GRANIT_BACKEND_READBACK_H_

#include <cstdint>
#include <memory>
#include <span>

#include <granit/renderer/readback_batch.h>

#include "backend/contracts/resources.h"

namespace granit::detail {

enum class backend_readback_type { buffer, texture };

struct backend_readback_operation {
  backend_readback_type type{backend_readback_type::buffer};
  const backend_buffer_resource* buffer{};
  const backend_texture_resource* texture{};
  std::uint64_t source_offset{};
  std::uint64_t size{};
  granit_texture_format format{GRANIT_TEXTURE_FORMAT_UNDEFINED};
  granit_texture_write_region texture_region{};
  granit_readback_result_info result_info = GRANIT_READBACK_RESULT_INFO_INIT;
};

/** 后端回读提交的非阻塞完成点；结果字节仅在 poll 成功后可复制。 */
class backend_readback_completion {
public:
  virtual ~backend_readback_completion() = default;
  [[nodiscard]] virtual granit_result poll() noexcept = 0;
  [[nodiscard]] virtual granit_result
  get_result_info(std::uint32_t index, granit_readback_result_info& info) const noexcept = 0;
  [[nodiscard]] virtual granit_result copy_result(std::uint32_t index, void* data,
                                                  std::uint64_t size) noexcept = 0;
};

} // namespace granit::detail

#endif
