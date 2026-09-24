// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "gltf/document_loader.h"

#include "assets/asset_location.h"
#include "assets/asset_request.h"
#include "gltf/document_manifest.h"

#include <new>
#include <utility>
#include <vector>

namespace granit::example::gltf {
namespace {

std::string batch_diagnostic(const assets::asset_batch& batch) {
  for (const auto& entry : batch.entries()) {
    if (entry.request && entry.request->status() == assets::asset_request_status::failed &&
        !entry.request->diagnostic().empty()) {
      return std::string{entry.request->diagnostic()};
    }
  }
  return "读取 glTF 外部资源失败";
}

document_load_error batch_error(const assets::asset_batch& batch) noexcept {
  for (const auto& entry : batch.entries()) {
    if (entry.request && entry.request->status() == assets::asset_request_status::failed &&
        entry.request->error() == assets::asset_request_error::out_of_memory) {
      return document_load_error::out_of_memory;
    }
  }
  return document_load_error::resource_read;
}

document_load_error manifest_error(document_manifest_error error) noexcept {
  switch (error) {
  case document_manifest_error::invalid_resource_uri:
    return document_load_error::invalid_location;
  case document_manifest_error::out_of_memory:
    return document_load_error::out_of_memory;
  case document_manifest_error::none:
  case document_manifest_error::invalid_document:
  case document_manifest_error::truncated_data:
    return document_load_error::invalid_document;
  }
  return document_load_error::invalid_document;
}

} // namespace

bool document_loader::start(std::string location) {
  if (status_ == document_load_status::loading_document ||
      status_ == document_load_status::loading_resources) {
    return false;
  }
  reset();
  location_ = std::move(location);
  document_request_ = loader_.load(location_);
  status_ = document_load_status::loading_document;
  return true;
}

void document_loader::poll() {
  loader_.poll();
  if (status_ == document_load_status::loading_document) {
    if (!document_request_) {
      fail(document_load_error::out_of_memory, "无法创建 glTF 文档请求");
      return;
    }
    if (document_request_->status() == assets::asset_request_status::failed) {
      auto error = document_load_error::document_read;
      if (document_request_->error() == assets::asset_request_error::invalid_location)
        error = document_load_error::invalid_location;
      else if (document_request_->error() == assets::asset_request_error::out_of_memory)
        error = document_load_error::out_of_memory;
      fail(error, std::string{document_request_->diagnostic()});
      return;
    }
    if (document_request_->status() == assets::asset_request_status::cancelled) {
      status_ = document_load_status::cancelled;
      error_ = document_load_error::cancelled;
      return;
    }
    if (document_request_->status() != assets::asset_request_status::ready)
      return;

    try {
      std::vector<std::string> resources;
      const auto discovery = discover_external_resources(document_request_->bytes(), resources);
      if (!discovery) {
        fail(manifest_error(discovery.error), discovery.diagnostic);
        return;
      }
      for (const auto& resource : resources) {
        std::string resource_location;
        if (!assets::resolve_asset_location(location_, resource, resource_location) ||
            !resource_batch_.add(resource, std::move(resource_location))) {
          fail(document_load_error::invalid_location, "glTF 外部资源位置无效");
          return;
        }
      }
      if (!resource_batch_.start(loader_)) {
        fail(document_load_error::resource_read, "无法启动 glTF 外部资源批次");
        return;
      }
      status_ = document_load_status::loading_resources;
    } catch (const std::bad_alloc&) {
      fail(document_load_error::out_of_memory, "准备 glTF 外部资源时内存不足");
      return;
    }
  }

  if (status_ != document_load_status::loading_resources)
    return;
  switch (resource_batch_.status()) {
  case assets::asset_batch_status::idle:
  case assets::asset_batch_status::pending:
    return;
  case assets::asset_batch_status::failed:
    fail(batch_error(resource_batch_), batch_diagnostic(resource_batch_));
    return;
  case assets::asset_batch_status::cancelled:
    status_ = document_load_status::cancelled;
    error_ = document_load_error::cancelled;
    return;
  case assets::asset_batch_status::ready:
    break;
  }
  if (!resource_batch_.commit(resolver_)) {
    fail(document_load_error::resource_read, "无法提交 glTF 外部资源");
    return;
  }
  status_ = document_load_status::ready;
}

void document_loader::cancel() noexcept {
  if (document_request_)
    document_request_->cancel();
  resource_batch_.cancel();
  if (status_ == document_load_status::loading_document ||
      status_ == document_load_status::loading_resources) {
    status_ = document_load_status::cancelled;
    error_ = document_load_error::cancelled;
    diagnostic_.clear();
  }
}

void document_loader::reset() noexcept {
  cancel();
  document_request_.reset();
  resource_batch_.clear();
  resolver_.clear();
  location_.clear();
  diagnostic_.clear();
  status_ = document_load_status::idle;
  error_ = document_load_error::none;
}

document_load_progress document_loader::progress() const noexcept {
  document_load_progress result;
  if (document_request_)
    result.document = document_request_->progress();
  result.resources = resource_batch_.progress();
  if (status_ == document_load_status::loading_resources ||
      status_ == document_load_status::ready) {
    result.stage = document_load_stage::resources;
  }
  return result;
}

std::span<const std::byte> document_loader::document() const noexcept {
  if (!document_request_ || status_ != document_load_status::ready)
    return {};
  return document_request_->bytes();
}

void document_loader::fail(document_load_error error, std::string diagnostic) {
  resource_batch_.cancel();
  error_ = error;
  diagnostic_ = std::move(diagnostic);
  status_ = document_load_status::failed;
}

} // namespace granit::example::gltf
