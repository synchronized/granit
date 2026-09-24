// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_DOCUMENT_LOADER_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_DOCUMENT_LOADER_H_

#include "assets/asset_batch.h"
#include "assets/asset_loader.h"
#include "assets/memory_resource_resolver.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace granit::example::gltf {

enum class document_load_status {
  idle,
  loading_document,
  loading_resources,
  ready,
  failed,
  cancelled
};

enum class document_load_error {
  none,
  invalid_location,
  document_read,
  invalid_document,
  resource_read,
  out_of_memory,
  cancelled,
};

enum class document_load_stage { document, resources };

struct document_load_progress {
  document_load_stage stage{document_load_stage::document};
  assets::asset_request_progress document;
  assets::asset_batch_progress resources;
};

/** 跨平台读取 glTF/GLB 主文档及其全部外部资源。 */
class document_loader {
public:
  document_loader() = default;
  document_loader(const document_loader&) = delete;
  document_loader& operator=(const document_loader&) = delete;

  /** 启动新请求；当前请求未结束时返回 false。 */
  [[nodiscard]] bool start(std::string location);
  /** 推进平台读取并发布状态变化。 */
  void poll();
  void cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] document_load_status status() const noexcept { return status_; }
  [[nodiscard]] document_load_error error() const noexcept { return error_; }
  [[nodiscard]] document_load_progress progress() const noexcept;
  [[nodiscard]] std::string_view location() const noexcept { return location_; }
  [[nodiscard]] std::string_view diagnostic() const noexcept { return diagnostic_; }
  [[nodiscard]] std::span<const std::byte> document() const noexcept;
  [[nodiscard]] const assets::resource_resolver& resolver() const noexcept { return resolver_; }

private:
  void fail(document_load_error error, std::string diagnostic);

  assets::asset_loader loader_;
  std::shared_ptr<assets::asset_request> document_request_;
  assets::asset_batch resource_batch_;
  assets::memory_resource_resolver resolver_;
  std::string location_;
  std::string diagnostic_;
  document_load_status status_{document_load_status::idle};
  document_load_error error_{document_load_error::none};
};

} // namespace granit::example::gltf

#endif
