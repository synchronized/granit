// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_system.h"

#include "assets/resource_path.h"

#include <filesystem>
#include <limits>
#include <utility>

namespace granit::example::assets {
namespace {

std::shared_ptr<asset_request> failed_request(std::string location, asset_request_error error,
                                              std::string diagnostic) {
  auto request = std::make_shared<asset_request>();
  const auto generation = asset_request_writer::begin(*request, std::move(location));
  static_cast<void>(asset_request_writer::fail(*request, generation, error, std::move(diagnostic)));
  return request;
}

bool resolve_from_root(std::string_view root, std::string_view logical_path, std::string& output) {
  std::string candidate;
  if (root.starts_with("http://") || root.starts_with("https://")) {
    candidate = std::string{root};
    const auto suffix = candidate.find_first_of("?#");
    if (suffix != candidate.npos)
      candidate.erase(suffix);
    if (candidate.empty())
      return false;
    if (candidate.back() != '/')
      candidate.push_back('/');
    candidate.append(logical_path);
  } else {
    candidate = (std::filesystem::path{std::string{root}} / std::filesystem::path{logical_path})
                    .lexically_normal()
                    .string();
  }
  if (candidate.empty())
    return false;
  output = std::move(candidate);
  return true;
}

} // namespace

bool asset_system::initialize(std::string_view executable_path) {
  if (bundled_.valid() || !store_.initialize(executable_path))
    return false;
  mounts_.push_back({.source = source_kind::bundled, .root_location = {}});
  bundled_ = asset_mount{1};
  return true;
}

bool asset_system::mount(std::string root_location, asset_mount& output) {
  if (!bundled_.valid() || root_location.empty() || root_location.find('\0') != std::string::npos ||
      mounts_.size() >= std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  mounts_.push_back({.source = source_kind::external, .root_location = std::move(root_location)});
  output = asset_mount{static_cast<std::uint32_t>(mounts_.size())};
  return true;
}

std::shared_ptr<asset_request> asset_system::request(asset_key key) {
  std::string normalized;
  if (!normalize_resource_path(key.path, normalized)) {
    return failed_request(std::string{key.path}, asset_request_error::invalid_location,
                          "资产逻辑路径无效");
  }
  const auto* record = find(key.mount);
  if (record == nullptr) {
    return failed_request(std::move(normalized), asset_request_error::invalid_location,
                          "资产挂载无效");
  }
  if (record->source == source_kind::bundled) {
    std::vector<std::byte> bytes;
    if (!store_.read(normalized, bytes)) {
      return failed_request("asset:///" + normalized, asset_request_error::io_error,
                            "无法读取打包资产");
    }
    auto request = std::make_shared<asset_request>();
    const auto generation = asset_request_writer::begin(*request, "asset:///" + normalized);
    static_cast<void>(asset_request_writer::complete(*request, generation, std::move(bytes)));
    return request;
  }

  std::string location;
  if (!resolve_from_root(record->root_location, normalized, location)) {
    return failed_request(std::move(normalized), asset_request_error::invalid_location,
                          "无法解析资产位置");
  }
  return loader_.load(std::move(location));
}

void asset_system::poll() { loader_.poll(); }

const asset_system::mount_record* asset_system::find(asset_mount mount) const noexcept {
  if (!mount.valid() || mount.value_ > mounts_.size())
    return nullptr;
  return &mounts_[mount.value_ - 1];
}

} // namespace granit::example::assets
