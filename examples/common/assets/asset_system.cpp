// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_system.h"

#include "assets/asset_loader.h"
#include "assets/asset_store.h"
#include "assets/resource_path.h"

#include <filesystem>
#include <limits>
#include <utility>
#include <vector>

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

struct asset_system::implementation {
  enum class source_kind { bundled, external };

  struct mount_record {
    source_kind source{};
    std::string root_location;
  };

  asset_store store;
  asset_loader loader;
  std::vector<mount_record> mounts;
};

asset_system::asset_system() : implementation_(std::make_unique<implementation>()) {}

asset_system::~asset_system() = default;

bool asset_system::initialize(std::string_view executable_path) {
  if (bundled_.valid() || !implementation_->store.initialize(executable_path))
    return false;
  implementation_->mounts.push_back(
      {.source = implementation::source_kind::bundled, .root_location = {}});
  bundled_ = asset_mount{1};
  return true;
}

bool asset_system::mount(std::string root_location, asset_mount& output) {
  if (!bundled_.valid() || root_location.empty() || root_location.find('\0') != std::string::npos ||
      implementation_->mounts.size() >= std::numeric_limits<std::uint32_t>::max()) {
    return false;
  }
  implementation_->mounts.push_back(
      {.source = implementation::source_kind::external,
       .root_location = std::move(root_location)});
  output = asset_mount{static_cast<std::uint32_t>(implementation_->mounts.size())};
  return true;
}

bool asset_system::mount_location(std::string location, asset_mount& output_mount,
                                  std::string& output_path) {
  if (location.empty() || location.find('\0') != std::string::npos)
    return false;

  std::string root;
  std::string path;
  if (location.starts_with("http://") || location.starts_with("https://")) {
    if (location.find_first_of("?#") != std::string::npos)
      return false;
    const auto separator = location.find_last_of('/');
    if (separator == std::string::npos || separator + 1 >= location.size())
      return false;
    root = location.substr(0, separator);
    path = location.substr(separator + 1);
  } else {
    const std::filesystem::path file{std::move(location)};
    path = file.filename().string();
    root = file.parent_path().string();
    if (root.empty())
      root = ".";
  }

  std::string normalized;
  if (!normalize_resource_path(path, normalized))
    return false;
  asset_mount mounted;
  if (!mount(std::move(root), mounted))
    return false;
  output_mount = mounted;
  output_path = std::move(normalized);
  return true;
}

std::shared_ptr<asset_request> asset_system::request(asset_key key) {
  std::string normalized;
  if (!normalize_resource_path(key.path, normalized)) {
    return failed_request(std::string{key.path}, asset_request_error::invalid_location,
                          "资产逻辑路径无效");
  }
  const implementation::mount_record* record = nullptr;
  if (key.mount.valid() && key.mount.value_ <= implementation_->mounts.size())
    record = &implementation_->mounts[key.mount.value_ - 1];
  if (record == nullptr) {
    return failed_request(std::move(normalized), asset_request_error::invalid_location,
                          "资产挂载无效");
  }
  if (record->source == implementation::source_kind::bundled) {
    std::vector<std::byte> bytes;
    if (!implementation_->store.read(normalized, bytes)) {
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
  return implementation_->loader.load(std::move(location));
}

void asset_system::poll() { implementation_->loader.poll(); }

} // namespace granit::example::assets
