// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "platform/register_asset_sources.h"

#include "assets/asset_manager.h"

#include <emscripten/fetch.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <new>
#include <utility>

namespace granit::example::platform {
namespace {

class memfs_asset_source final : public assets::asset_source {
public:
  explicit memfs_asset_source(std::filesystem::path root = {}) : root_(std::move(root)) {}

  [[nodiscard]] granit::result load(const assets::asset_location& location,
                                    assets::asset_source_completion completion) noexcept override {
    try {
      const auto path = root_.empty() ? std::filesystem::path{location.path()}
                                      : root_ / std::filesystem::path{location.path()};
      std::ifstream stream{path, std::ios::binary | std::ios::ate};
      if (!stream) {
        completion({.error = assets::asset_error::io_error,
                    .bytes = {},
                    .total_bytes = std::nullopt,
                    .diagnostic = "无法打开 MEMFS 资产"});
        return granit::result::success;
      }
      const auto end = stream.tellg();
      if (end < 0 || static_cast<std::uint64_t>(end) >
                         static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        completion({.error = assets::asset_error::io_error,
                    .bytes = {},
                    .total_bytes = std::nullopt,
                    .diagnostic = "MEMFS 资产大小无效"});
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
                    .diagnostic = "无法完整读取 MEMFS 资产"});
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

struct fetch_context {
  assets::asset_source_completion completion;
};

void fetch_success(emscripten_fetch_t* fetch) {
  std::unique_ptr<fetch_context> context{static_cast<fetch_context*>(fetch->userData)};
  try {
    std::vector<std::byte> bytes(static_cast<std::size_t>(fetch->numBytes));
    if (!bytes.empty())
      std::memcpy(bytes.data(), fetch->data, bytes.size());
    context->completion({.error = assets::asset_error::none,
                         .bytes = std::move(bytes),
                         .total_bytes = static_cast<std::uint64_t>(fetch->numBytes),
                         .diagnostic = {}});
  } catch (const std::bad_alloc&) {
    context->completion({.error = assets::asset_error::out_of_memory,
                         .bytes = {},
                         .total_bytes = std::nullopt,
                         .diagnostic = "无法保存 Fetch 响应"});
  } catch (...) {
    context->completion({.error = assets::asset_error::internal,
                         .bytes = {},
                         .total_bytes = std::nullopt,
                         .diagnostic = "Fetch 完成回调发生内部错误"});
  }
  emscripten_fetch_close(fetch);
}

void fetch_failure(emscripten_fetch_t* fetch) {
  std::unique_ptr<fetch_context> context{static_cast<fetch_context*>(fetch->userData)};
  context->completion({.error = assets::asset_error::transport_error,
                       .bytes = {},
                       .total_bytes = std::nullopt,
                       .diagnostic = "HTTP 资产请求失败"});
  emscripten_fetch_close(fetch);
}

class fetch_asset_source final : public assets::asset_source {
public:
  [[nodiscard]] granit::result load(const assets::asset_location& location,
                                    assets::asset_source_completion completion) noexcept override {
    try {
      auto context = std::make_unique<fetch_context>();
      context->completion = std::move(completion);
      emscripten_fetch_attr_t attributes;
      emscripten_fetch_attr_init(&attributes);
      std::memcpy(attributes.requestMethod, "GET", 4);
      attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
      attributes.onsuccess = fetch_success;
      attributes.onerror = fetch_failure;
      attributes.userData = context.get();
      const std::string url{location.path()};
      if (emscripten_fetch(&attributes, url.c_str()) == nullptr)
        return granit::result::internal;
      static_cast<void>(context.release());
      return granit::result::success;
    } catch (const std::bad_alloc&) {
      return granit::result::out_of_memory;
    } catch (...) {
      return granit::result::internal;
    }
  }
};

} // namespace

granit::result register_asset_sources(assets::asset_manager& manager,
                                      std::string_view executable_path) noexcept {
  static_cast<void>(executable_path);
  try {
    auto result = manager.register_source(assets::asset_scheme::bundled,
                                          std::make_shared<memfs_asset_source>("/assets"));
    if (result.ok())
      result = manager.register_source(assets::asset_scheme::file,
                                       std::make_shared<fetch_asset_source>());
    if (result.ok())
      result = manager.register_source(assets::asset_scheme::http,
                                       std::make_shared<fetch_asset_source>());
    if (result.ok()) {
      result = manager.register_source(assets::asset_scheme::https,
                                       std::make_shared<fetch_asset_source>());
    }
    return result;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  } catch (...) {
    return granit::result::internal;
  }
}

} // namespace granit::example::platform
