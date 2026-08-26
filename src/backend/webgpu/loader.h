// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_WEBGPU_LOADER_H_
#define GRANIT_BACKEND_WEBGPU_LOADER_H_

#include <granit/core/result.h>

namespace granit::detail {

/** 管理桌面 WebGPU Provider 动态库及其基础入口。 */
class webgpu_loader {
public:
  using create_instance_fn = void* (*)(const void* descriptor);

  webgpu_loader() = default;
  ~webgpu_loader();

  webgpu_loader(const webgpu_loader&) = delete;
  webgpu_loader& operator=(const webgpu_loader&) = delete;

  /** 加载指定 Provider；失败后对象保持关闭状态。 */
  [[nodiscard]] granit_result open(const char* library_path) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return handle_ != nullptr; }
  [[nodiscard]] create_instance_fn create_instance() const noexcept { return create_instance_; }

private:
  void* handle_{nullptr};
  create_instance_fn create_instance_{nullptr};
};

} // namespace granit::detail

#endif
