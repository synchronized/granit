// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/register_asset_sources.h"

#include "assets/asset_manager.h"

#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace granit::example::platform {
namespace {

class filesystem_asset_source final : public assets::asset_source {
public:
  explicit filesystem_asset_source(std::filesystem::path root = {}) : root_(std::move(root)) {}

  [[nodiscard]] granit::result
  load(const assets::asset_location& location,
       assets::asset_source_completion completion) noexcept override {
    try {
      const auto path = root_.empty() ? std::filesystem::path{location.path()}
                                      : root_ / std::filesystem::path{location.path()};
      std::ifstream stream{path, std::ios::binary | std::ios::ate};
      if (!stream) {
        completion({.error = assets::asset_error::io_error,
                    .bytes = {},
                    .total_bytes = std::nullopt,
                    .diagnostic = "无法打开资产文件"});
        return granit::result::success;
      }
      const auto end = stream.tellg();
      if (end < 0 || static_cast<std::uint64_t>(end) >
                         static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        completion({.error = assets::asset_error::io_error,
                    .bytes = {},
                    .total_bytes = std::nullopt,
                    .diagnostic = "资产文件大小无效"});
        return granit::result::success;
      }
      const auto size = static_cast<std::size_t>(end);
      std::vector<std::byte> bytes(size);
      stream.seekg(0, std::ios::beg);
      if (size != 0)
        stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(size));
      if (!stream) {
        completion({.error = assets::asset_error::io_error,
                    .bytes = {},
                    .total_bytes = size,
                    .diagnostic = "无法完整读取资产文件"});
        return granit::result::success;
      }
      completion({.error = assets::asset_error::none,
                  .bytes = std::move(bytes),
                  .total_bytes = size,
                  .diagnostic = {}});
      return granit::result::success;
    } catch (const std::bad_alloc&) {
      return granit::result::out_of_memory;
    } catch (...) {
      return granit::result::internal;
    }
  }

private:
  std::filesystem::path root_;
};

} // namespace

granit::result register_asset_sources(assets::asset_manager& manager,
                                      std::string_view executable_path) noexcept {
  try {
    if (executable_path.empty())
      return granit::result::invalid_argument;
    std::error_code error;
    const auto executable =
        std::filesystem::absolute(std::filesystem::path{executable_path}, error);
    if (error)
      return granit::result::invalid_argument;
    auto result = manager.register_source(
        assets::asset_scheme::bundled,
        std::make_shared<filesystem_asset_source>(executable.parent_path() / "assets"));
    if (result.ok()) {
      result = manager.register_source(assets::asset_scheme::file,
                                       std::make_shared<filesystem_asset_source>());
    }
    return result;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

} // namespace granit::example::platform
