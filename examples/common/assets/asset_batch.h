// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_BATCH_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_BATCH_H_

#include "assets/asset_system.h"
#include "assets/memory_resource_resolver.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace granit::example::assets {

enum class asset_batch_status { idle, pending, ready, failed, cancelled };

struct asset_batch_progress {
  std::size_t completed_items{};
  std::size_t total_items{};
  std::uint64_t received_bytes{};
  std::optional<std::uint64_t> total_bytes;
};

struct asset_batch_entry {
  std::string resource_uri;
  asset_mount source_mount;
  std::string source_path;
  std::shared_ptr<asset_request> request;
};

/** 并行读取一组资源，并在全部完成后原子构造内存 resolver。 */
class asset_batch {
public:
  /** 添加 Resolver 查询 URI 与已经由上层解析完成的资产 Key。 */
  [[nodiscard]] bool add(std::string_view resource_uri, asset_key source);
  [[nodiscard]] bool start(asset_system& assets);
  [[nodiscard]] asset_batch_status status() const noexcept;
  [[nodiscard]] asset_batch_progress progress() const noexcept;
  [[nodiscard]] bool commit(memory_resource_resolver& resolver) const;
  void cancel() noexcept;
  void clear() noexcept;

  [[nodiscard]] const std::vector<asset_batch_entry>& entries() const noexcept { return entries_; }

private:
  std::vector<asset_batch_entry> entries_;
  bool started_{};
};

} // namespace granit::example::assets

#endif
