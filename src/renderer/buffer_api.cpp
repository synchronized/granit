// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/buffer.h>

#include "core/resource_validation.h"
#include "renderer/renderer_registry.h"

namespace {

void report_validation(granit_renderer renderer, std::string_view message) noexcept {
  granit::detail::renderer_registry::instance().emit_validation_diagnostic(renderer, message);
}

} // namespace

extern "C" granit_result granit_buffer_create(granit_renderer renderer,
                                              const granit_buffer_desc* desc,
                                              granit_buffer* buffer) {
  if (desc == nullptr || buffer == nullptr) {
    report_validation(renderer,
                      "granit_buffer_create: desc 和 buffer 输出参数均不得为空");
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *buffer = GRANIT_NULL_HANDLE;
  if (renderer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  const auto validation_result = granit::detail::validate_buffer_desc(*desc);
  if (validation_result != GRANIT_SUCCESS) {
    report_validation(renderer,
                      "granit_buffer_create: desc 的结构版本、保留字段、大小、用途或内存位置无效");
    return validation_result;
  }
  try {
    return granit::detail::renderer_registry::instance().create_buffer(renderer, *desc, *buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_map(granit_renderer renderer, granit_buffer buffer,
                                           uint64_t offset, uint64_t size, void** data) {
  if (data == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *data = nullptr;
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().map_buffer(renderer, buffer, offset, size,
                                                                    *data);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result
granit_buffer_create_with_data(granit_renderer renderer, const granit_buffer_desc* desc,
                               const granit_buffer_initial_data* initial_data,
                               granit_buffer* buffer) {
  if (desc == nullptr || initial_data == nullptr || buffer == nullptr) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *buffer = GRANIT_NULL_HANDLE;
  const auto validation_result = granit::detail::validate_buffer_desc(*desc);
  if (validation_result != GRANIT_SUCCESS) {
    return validation_result;
  }
  if (initial_data->struct_size < GRANIT_BUFFER_INITIAL_DATA_VERSION_1_SIZE ||
      initial_data->reserved != 0 || initial_data->data == nullptr ||
      initial_data->size != desc->size || initial_data->size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (desc->memory_location == GRANIT_MEMORY_LOCATION_READBACK) {
    return GRANIT_ERROR_UNSUPPORTED;
  }
  granit_buffer created = GRANIT_NULL_HANDLE;
  const auto create_result = granit_buffer_create(renderer, desc, &created);
  if (create_result != GRANIT_SUCCESS) {
    return create_result;
  }
  const auto write_result =
      granit_buffer_write(renderer, created, 0, initial_data->data, initial_data->size);
  if (write_result != GRANIT_SUCCESS) {
    static_cast<void>(granit_buffer_destroy(renderer, created));
    return write_result;
  }
  *buffer = created;
  return GRANIT_SUCCESS;
}

extern "C" granit_result granit_buffer_write(granit_renderer renderer, granit_buffer buffer,
                                             uint64_t offset, const void* data, uint64_t size) {
  if (data == nullptr || size == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().write_buffer(renderer, buffer, offset,
                                                                      data, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_unmap(granit_renderer renderer, granit_buffer buffer) {
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return granit::detail::renderer_registry::instance().unmap_buffer(renderer, buffer);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_flush(granit_renderer renderer, granit_buffer buffer,
                                             uint64_t offset, uint64_t size) {
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().flush_mapped_buffer(renderer, buffer,
                                                                             offset, size);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_get_desc(granit_renderer renderer, granit_buffer buffer,
                                                granit_buffer_desc* desc) {
  if (desc == nullptr)
    return GRANIT_ERROR_INVALID_ARGUMENT;
  *desc = {};
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE)
    return GRANIT_ERROR_INVALID_HANDLE;
  try {
    return granit::detail::renderer_registry::instance().get_buffer_desc(renderer, buffer, *desc);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

extern "C" granit_result granit_buffer_destroy(granit_renderer renderer, granit_buffer buffer) {
  if (renderer == GRANIT_NULL_HANDLE || buffer == GRANIT_NULL_HANDLE) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    auto& registry = granit::detail::renderer_registry::instance();
    const auto result = registry.destroy_buffer(renderer, buffer);
    if (result == GRANIT_ERROR_INVALID_HANDLE) {
      registry.emit_validation_diagnostic(
          renderer,
          "granit_buffer_destroy: buffer 必须是有效的 Buffer 句柄并属于指定 Renderer");
    } else if (result == GRANIT_ERROR_INVALID_ARGUMENT) {
      registry.emit_validation_diagnostic(renderer,
                                          "granit_buffer_destroy: buffer 仍处于映射状态");
    }
    return result;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}
