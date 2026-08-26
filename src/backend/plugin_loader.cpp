// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "backend/plugin_loader.h"

#include <algorithm>
#include <cstddef>
#include <new>

namespace granit::detail {
namespace {

bool is_compatible(const granit_backend_plugin_api* api,
                   granit_backend_plugin_kind expected_kind) noexcept {
  constexpr std::size_t minimum_size = offsetof(granit_backend_plugin_api, instance_api) +
                                       sizeof(const granit_backend_plugin_instance_api*);
  constexpr std::size_t minimum_instance_api_size =
      offsetof(granit_backend_plugin_instance_api, get_capabilities) +
      sizeof(granit_backend_plugin_get_capabilities_fn);
  return api != nullptr && api->struct_size >= minimum_size &&
         api->abi_version == GRANIT_BACKEND_PLUGIN_ABI_VERSION && api->kind == expected_kind &&
         api->reserved == 0 && api->name != nullptr && api->name_length != 0 &&
         api->create != nullptr && api->destroy != nullptr && api->instance_api != nullptr &&
         api->instance_api->struct_size >= minimum_instance_api_size &&
         api->instance_api->reserved == 0 && api->instance_api->get_capabilities != nullptr;
}

bool is_valid_host(const granit_backend_plugin_host_api* host) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_host_api, allocator_user_data) + sizeof(void*);
  return host != nullptr && host->struct_size >= minimum_size && host->reserved == 0 &&
         host->allocate != nullptr && host->deallocate != nullptr;
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

granit_result
backend_plugin_loader::create_instance(const granit_backend_plugin_host_api* host,
                                       granit_backend_plugin_instance* out_instance) noexcept {
  if (api_ == nullptr || out_instance == nullptr || !is_valid_host(host)) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  *out_instance = 0;

  granit_backend_plugin_instance instance = 0;
  granit_result result = GRANIT_ERROR_INTERNAL;
  try {
    result = api_->create(host, &instance);
  } catch (...) {
    if (instance != 0) {
      try {
        api_->destroy(instance);
      } catch (...) {
      }
    }
    return GRANIT_ERROR_INTERNAL;
  }
  if (result != GRANIT_SUCCESS) {
    if (instance != 0) {
      try {
        api_->destroy(instance);
      } catch (...) {
      }
    }
    return result;
  }
  if (instance == 0) {
    return GRANIT_ERROR_INITIALIZATION_FAILED;
  }

  try {
    instances_.push_back(instance);
  } catch (const std::bad_alloc&) {
    try {
      api_->destroy(instance);
    } catch (...) {
    }
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    try {
      api_->destroy(instance);
    } catch (...) {
    }
    return GRANIT_ERROR_INTERNAL;
  }
  *out_instance = instance;
  return GRANIT_SUCCESS;
}

granit_result
backend_plugin_loader::destroy_instance(granit_backend_plugin_instance instance) noexcept {
  if (api_ == nullptr || instance == 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  const auto found = std::find(instances_.begin(), instances_.end(), instance);
  if (found == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    api_->destroy(instance);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
  instances_.erase(found);
  return GRANIT_SUCCESS;
}

granit_result
backend_plugin_loader::get_capabilities(granit_backend_plugin_instance instance,
                                        granit_backend_plugin_capabilities* capabilities) noexcept {
  constexpr std::size_t minimum_size =
      offsetof(granit_backend_plugin_capabilities, max_color_attachments) + sizeof(std::uint32_t);
  if (api_ == nullptr || instance == 0 || capabilities == nullptr ||
      capabilities->struct_size < minimum_size || capabilities->reserved != 0) {
    return GRANIT_ERROR_INVALID_ARGUMENT;
  }
  if (std::find(instances_.begin(), instances_.end(), instance) == instances_.end()) {
    return GRANIT_ERROR_INVALID_HANDLE;
  }
  try {
    return api_->instance_api->get_capabilities(instance, capabilities);
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

void backend_plugin_loader::close() noexcept {
  if (api_ != nullptr) {
    for (auto iterator = instances_.rbegin(); iterator != instances_.rend(); ++iterator) {
      try {
        api_->destroy(*iterator);
      } catch (...) {
      }
    }
  }
  instances_.clear();
  api_ = nullptr;
  library_.close();
}

} // namespace granit::detail
