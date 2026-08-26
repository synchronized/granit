// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstddef>
#include <new>

#include "backend/plugin_loader.h"

namespace {

void* allocate(uint64_t size, uint64_t alignment, void*) {
  return ::operator new(static_cast<std::size_t>(size),
                        std::align_val_t{static_cast<std::size_t>(alignment)}, std::nothrow);
}

void deallocate(void* memory, uint64_t, uint64_t alignment, void*) {
  ::operator delete(memory, std::align_val_t{static_cast<std::size_t>(alignment)});
}

void diagnose(granit_diagnostic_severity, granit_diagnostic_category, const char*, uint32_t,
              void*) {}

} // namespace

int main() {
  granit::detail::backend_plugin_loader loader;
  if (loader.open(GRANIT_WEBGPU_PLUGIN_PATH, GRANIT_BACKEND_PLUGIN_KIND_WEBGPU) != GRANIT_SUCCESS) {
    return 1;
  }

  granit_backend_plugin_host_api host{
      sizeof(granit_backend_plugin_host_api), 0, diagnose, nullptr, allocate, deallocate, nullptr};
  granit_backend_plugin_instance instance{};
  if (loader.create_instance(&host, &instance) != GRANIT_SUCCESS || instance == 0) {
    return 2;
  }
  return loader.destroy_instance(instance) == GRANIT_SUCCESS ? 0 : 3;
}
