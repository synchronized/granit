// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOCATION_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_LOCATION_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace granit::example::assets {

enum class asset_scheme : std::uint32_t { bundled, file, http, https, memory };

/** 与平台路径解析分离的资产位置。 */
class asset_location final {
public:
  asset_location() = default;

  [[nodiscard]] static asset_location bundled(std::string path);
  [[nodiscard]] static asset_location external(std::string location);
  [[nodiscard]] static asset_location memory(std::string path);

  [[nodiscard]] bool valid() const noexcept { return valid_; }
  [[nodiscard]] asset_scheme scheme() const noexcept { return scheme_; }
  [[nodiscard]] std::string_view path() const noexcept { return path_; }
  [[nodiscard]] std::string key() const;
  [[nodiscard]] asset_location resolve(std::string_view relative_path) const;

private:
  asset_location(asset_scheme scheme, std::string path, bool valid) noexcept
      : scheme_(scheme), path_(std::move(path)), valid_(valid) {}

  asset_scheme scheme_{asset_scheme::bundled};
  std::string path_;
  bool valid_{};
};

} // namespace granit::example::assets

#endif
