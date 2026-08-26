// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_loader.h"

#include <cstddef>

namespace granit::detail {
namespace {

bool is_compatible(const granit_backend_plugin_api* api,
                   granit_backend_plugin_kind expected_kind) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_api, name_length) + sizeof(uint32_t);
  return api != nullptr && api->struct_size >= minimum_size &&
         api->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION && api->kind == expected_kind &&
         api->reserved == 0 && api->name != nullptr && api->name_length != 0;
}

} // namespace

backend_plugin_loader::~backend_plugin_loader() { close(); }

granit_result backend_plugin_loader::open(const char* library_path,
                                          granit_backend_plugin_kind expected_kind) noexcept {
  close();
  if (library_path == nullptr || library_path[0] == '\0') {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }

  if (!library_.open(library_path)) {
    return GRANIT_ERROR_BACKEND_UNAVAILABLE;
  }

  const auto query = reinterpret_cast<granit_backend_plugin_query_fn>(
      library_.symbol(GRANIT_BACKEND_PLUGIN_QUERY_SYMBOL));
  if (query == nullptr) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }

  api_ = query(GRANIT_BACKEND_PLUGIN_ABI_VERSION);
  if (!is_compatible(api_, expected_kind)) {
    close();
    return GRANIT_ERROR_INCOMPATIBLE_DRIVER;
  }
  return GRANIT_SUCCESS;
}

void backend_plugin_loader::close() noexcept {
  api_ = nullptr;
  library_.close();
}

} // namespace granit::detail
