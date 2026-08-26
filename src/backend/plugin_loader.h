// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_BACKEND_PLUGIN_LOADER_H_
#define GRANIT_BACKEND_PLUGIN_LOADER_H_

#include <granit/core/result.h>

#include "backend/plugin_api.h"
#include "platform/shared_library.h"

namespace granit::detail {

/** 加载并校验 Granit 自有后端插件。 */
class backend_plugin_loader {
public:
  backend_plugin_loader() = default;
  ~backend_plugin_loader();

  backend_plugin_loader(const backend_plugin_loader&) = delete;
  backend_plugin_loader& operator=(const backend_plugin_loader&) = delete;

  /** 加载指定插件；失败后对象保持关闭状态。 */
  [[nodiscard]] granit_result open(const char* library_path,
                                   granit_backend_plugin_kind expected_kind) noexcept;
  void close() noexcept;

  [[nodiscard]] bool is_open() const noexcept { return library_.is_open(); }
  [[nodiscard]] const granit_backend_plugin_api* api() const noexcept { return api_; }

private:
  platform::shared_library library_;
  const granit_backend_plugin_api* api_{nullptr};
};

} // namespace granit::detail

#endif
