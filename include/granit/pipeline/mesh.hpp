// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_PIPELINE_MESH_HPP_
#define GRANIT_PIPELINE_MESH_HPP_

#include <granit/core/result.hpp>
#include <granit/pipeline/mesh.h>

#include <utility>

namespace granit {

/** 公共 Mesh C ABI 的轻量 move-only RAII 包装。 */
class mesh {
public:
  mesh() = default;
  ~mesh() { static_cast<void>(reset()); }
  mesh(const mesh&) = delete;
  mesh& operator=(const mesh&) = delete;
  mesh(mesh&& other) noexcept
      : renderer_(std::exchange(other.renderer_, GRANIT_NULL_HANDLE)),
        handle_(std::exchange(other.handle_, GRANIT_NULL_HANDLE)) {}
  mesh& operator=(mesh&& other) noexcept {
    if (this != &other) {
      static_cast<void>(reset());
      renderer_ = std::exchange(other.renderer_, GRANIT_NULL_HANDLE);
      handle_ = std::exchange(other.handle_, GRANIT_NULL_HANDLE);
    }
    return *this;
  }

  [[nodiscard]] result initialize(granit_renderer renderer, const granit_mesh_desc& desc) noexcept {
    if (valid())
      return result::invalid_argument;
    const auto value = from_native(granit_mesh_create(renderer, &desc, &handle_));
    if (succeeded(value))
      renderer_ = renderer;
    return value;
  }
  [[nodiscard]] result reset() noexcept {
    if (!valid())
      return result::success;
    const auto renderer = std::exchange(renderer_, GRANIT_NULL_HANDLE);
    const auto handle = std::exchange(handle_, GRANIT_NULL_HANDLE);
    return from_native(granit_mesh_destroy(renderer, handle));
  }
  [[nodiscard]] bool valid() const noexcept { return handle_ != GRANIT_NULL_HANDLE; }
  [[nodiscard]] granit_mesh native_handle() const noexcept { return handle_; }

private:
  granit_renderer renderer_ = GRANIT_NULL_HANDLE;
  granit_mesh handle_ = GRANIT_NULL_HANDLE;
};

} // namespace granit

#endif
