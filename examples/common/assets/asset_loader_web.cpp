// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_loader.h"

#include <emscripten/fetch.h>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>
#include <new>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace granit::example::assets {
namespace {

struct fetch_context {
  std::shared_ptr<asset_request> request;
  std::uint64_t generation{};
};

void finish_fetch(emscripten_fetch_t* fetch) noexcept {
  auto context = std::unique_ptr<fetch_context>(static_cast<fetch_context*>(fetch->userData));
  emscripten_fetch_close(fetch);
}

void fetch_progress(emscripten_fetch_t* fetch) noexcept {
  auto* context = static_cast<fetch_context*>(fetch->userData);
  const auto received = fetch->dataOffset > 0 ? static_cast<std::uint64_t>(fetch->dataOffset) : 0;
  const auto total =
      fetch->totalBytes > 0
          ? std::optional<std::uint64_t>{static_cast<std::uint64_t>(fetch->totalBytes)}
          : std::nullopt;
  asset_request_writer::progress(*context->request, context->generation, received, total);
}

void fetch_succeeded(emscripten_fetch_t* fetch) noexcept {
  auto* context = static_cast<fetch_context*>(fetch->userData);
  if (fetch->data != nullptr && fetch->numBytes > 0 &&
      fetch->numBytes <= std::numeric_limits<std::size_t>::max()) {
    const auto* data = reinterpret_cast<const std::byte*>(fetch->data);
    static_cast<void>(asset_request_writer::complete(
        *context->request, context->generation,
        std::span<const std::byte>(data, static_cast<std::size_t>(fetch->numBytes))));
  } else {
    static_cast<void>(asset_request_writer::fail(*context->request, context->generation,
                                                 asset_request_error::transport_error,
                                                 "资源响应为空"));
  }
  finish_fetch(fetch);
}

void fetch_failed(emscripten_fetch_t* fetch) noexcept {
  auto* context = static_cast<fetch_context*>(fetch->userData);
  std::string diagnostic;
  if (fetch->status == 0) {
    diagnostic = "资源请求失败（状态码 0）：可能是 CORS、网络或证书错误";
  } else {
    diagnostic = "HTTP 请求失败：" + std::to_string(fetch->status);
  }
  static_cast<void>(asset_request_writer::fail(*context->request, context->generation,
                                               asset_request_error::transport_error,
                                               std::move(diagnostic)));
  finish_fetch(fetch);
}

} // namespace

struct asset_loader::implementation {};

asset_loader::asset_loader() : implementation_(std::make_unique<implementation>()) {}
asset_loader::~asset_loader() = default;

std::shared_ptr<asset_request> asset_loader::load(std::string location) {
  auto request = std::make_shared<asset_request>();
  const auto generation = asset_request_writer::begin(*request, location);
  if (location.empty() || location.find('\0') != std::string::npos) {
    static_cast<void>(asset_request_writer::fail(
        *request, generation, asset_request_error::invalid_location, "资产位置为空或包含 NUL"));
    return request;
  }

  auto context =
      std::unique_ptr<fetch_context>(new (std::nothrow) fetch_context{request, generation});
  if (!context) {
    static_cast<void>(asset_request_writer::fail(
        *request, generation, asset_request_error::out_of_memory, "无法分配 Fetch 回调状态"));
    return request;
  }

  emscripten_fetch_attr_t attributes;
  emscripten_fetch_attr_init(&attributes);
  std::snprintf(attributes.requestMethod, sizeof(attributes.requestMethod), "GET");
  attributes.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
  attributes.onsuccess = fetch_succeeded;
  attributes.onerror = fetch_failed;
  attributes.onprogress = fetch_progress;
  attributes.userData = context.get();
  if (emscripten_fetch(&attributes, location.c_str()) == nullptr) {
    static_cast<void>(asset_request_writer::fail(
        *request, generation, asset_request_error::transport_error, "无法启动 Fetch 请求"));
    return request;
  }
  static_cast<void>(context.release());
  return request;
}

void asset_loader::poll() {}

} // namespace granit::example::assets
