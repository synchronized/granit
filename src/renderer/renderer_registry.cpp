// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_registry.h"

#include <new>
#include <utility>

namespace granit::detail {

renderer_registry& renderer_registry::instance() {
  static renderer_registry registry;
  return registry;
}

granit_result renderer_registry::create(
  std::string_view application_name,
  bool enable_validation,
  granit_renderer& renderer) {
  try {
    auto state = std::make_shared<renderer_state>();
    const auto initialize_result = state->initialize(application_name, enable_validation);
    if (initialize_result != GRANIT_SUCCESS) {
      return initialize_result;
    }

    std::lock_guard lock{mutex_};
    state->set_domain(allocate_domain());
    const auto handle = handles_.insert(state.get(), resource_type::renderer, 0);
    if (handle == GRANIT_NULL_HANDLE) {
      return GRANIT_ERROR_OUT_OF_MEMORY;
    }
    try {
      renderers_.emplace(handle, std::move(state));
    } catch (...) {
      static_cast<void>(handles_.erase(handle, resource_type::renderer, 0));
      throw;
    }
    renderer = handle;
    return GRANIT_SUCCESS;
  } catch (const std::bad_alloc&) {
    return GRANIT_ERROR_OUT_OF_MEMORY;
  } catch (...) {
    return GRANIT_ERROR_INTERNAL;
  }
}

granit_result renderer_registry::destroy(granit_renderer renderer) {
  std::shared_ptr<renderer_state> state;
  {
    std::lock_guard lock{mutex_};
    if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
      return GRANIT_ERROR_INVALID_HANDLE;
    }
    auto found = renderers_.find(renderer);
    if (found == renderers_.end()) {
      return GRANIT_ERROR_INTERNAL;
    }
    state = std::move(found->second);
    renderers_.erase(found);
    const auto erase_result = handles_.erase(renderer, resource_type::renderer, 0);
    if (erase_result != GRANIT_SUCCESS) {
      return erase_result;
    }
  }
  // 析构可能等待 GPU 空闲，不应占用全局 registry 锁。
  state.reset();
  return GRANIT_SUCCESS;
}

std::shared_ptr<renderer_state> renderer_registry::acquire(granit_renderer renderer) {
  std::lock_guard lock{mutex_};
  if (handles_.find(renderer, resource_type::renderer, 0) == nullptr) {
    return {};
  }
  const auto found = renderers_.find(renderer);
  return found == renderers_.end() ? std::shared_ptr<renderer_state>{} : found->second;
}

std::uint32_t renderer_registry::allocate_domain() noexcept {
  const auto domain = next_domain_++;
  if (next_domain_ == 0) {
    next_domain_ = 1;
  }
  return domain == 0 ? allocate_domain() : domain;
}

} // namespace granit::detail
