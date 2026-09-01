// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "model_viewer_fetch.h"

#include <emscripten/fetch.h>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <new>
#include <span>
#include <string>

namespace granit::example::model_viewer::web {
namespace {

struct fetch_context {
  std::shared_ptr<asset_request> request;
  std::uint64_t generation{};
};

void finish_fetch(emscripten_fetch_t* fetch) noexcept {
  auto context = std::unique_ptr<fetch_context>(static_cast<fetch_context*>(fetch->userData));
  emscripten_fetch_close(fetch);
}

void fetch_succeeded(emscripten_fetch_t* fetch) noexcept {
  auto* context = static_cast<fetch_context*>(fetch->userData);
  if (fetch->data != nullptr && fetch->numBytes > 0 &&
      fetch->numBytes <= std::numeric_limits<std::size_t>::max()) {
    const auto* data = reinterpret_cast<const std::byte*>(fetch->data);
    static_cast<void>(context->request->complete(
        context->generation,
        std::span<const std::byte>(data, static_cast<std::size_t>(fetch->numBytes))));
  } else {
    static_cast<void>(context->request->fail(context->generation, "资源响应为空"));
  }
  finish_fetch(fetch);
}

void fetch_failed(emscripten_fetch_t* fetch) noexcept {
  auto* context = static_cast<fetch_context*>(fetch->userData);
  auto diagnostic = std::string("HTTP 请求失败：") + std::to_string(fetch->status);
  static_cast<void>(context->request->fail(context->generation, std::move(diagnostic)));
  finish_fetch(fetch);
}

} // namespace

bool start_fetch(const std::shared_ptr<asset_request>& request, std::string_view url) {
  if (!request || url.empty() || url.find('\0') != std::string_view::npos)
    return false;

  std::string stable_url(url);
  const auto generation = request->begin(stable_url);
  auto context =
      std::unique_ptr<fetch_context>(new (std::nothrow) fetch_context{request, generation});
  if (!context) {
    static_cast<void>(request->fail(generation, "无法分配 Fetch 回调状态"));
    return false;
  }

  emscripten_fetch_attr_t attributes;
  emscripten_fetch_attr_init(&attributes);
  std::snprintf(attributes.requestMethod, sizeof(attributes.requestMethod), "GET");
  attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attributes.onsuccess = fetch_succeeded;
  attributes.onerror = fetch_failed;
  attributes.userData = context.get();
  if (emscripten_fetch(&attributes, stable_url.c_str()) == nullptr) {
    static_cast<void>(request->fail(generation, "无法启动 Fetch 请求"));
    return false;
  }
  static_cast<void>(context.release());
  return true;
}

} // namespace granit::example::model_viewer::web
