// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_loader.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace granit::example::assets {
namespace {

struct load_result {
  std::shared_ptr<asset_request> request;
  std::uint64_t generation{};
  std::vector<std::byte> bytes;
  asset_request_error error{asset_request_error::none};
  std::string diagnostic;
};

load_result read_asset(std::shared_ptr<asset_request> request, std::uint64_t generation,
                       const std::string& location) {
  load_result result{.request = std::move(request),
                     .generation = generation,
                     .bytes = {},
                     .error = asset_request_error::none,
                     .diagnostic = {}};
  std::ifstream stream(std::filesystem::path{location}, std::ios::binary | std::ios::ate);
  if (!stream) {
    result.error = asset_request_error::io_error;
    result.diagnostic = "无法打开资产文件";
    return result;
  }
  const auto end = stream.tellg();
  if (end <= 0 || static_cast<std::uintmax_t>(end) > std::numeric_limits<std::size_t>::max()) {
    result.error = asset_request_error::io_error;
    result.diagnostic = "资产文件为空或尺寸超出限制";
    return result;
  }
  const auto size = static_cast<std::size_t>(end);
  result.bytes.resize(size);
  stream.seekg(0, std::ios::beg);

  constexpr std::size_t chunk_size = 256 * 1024;
  std::size_t offset = 0;
  while (offset < size) {
    if (!asset_request_writer::active(*result.request, generation)) {
      result.bytes.clear();
      return result;
    }
    const auto count = std::min(chunk_size, size - offset);
    stream.read(reinterpret_cast<char*>(result.bytes.data() + offset),
                static_cast<std::streamsize>(count));
    if (!stream) {
      result.bytes.clear();
      result.error = asset_request_error::io_error;
      result.diagnostic = "读取资产文件失败";
      return result;
    }
    offset += count;
    asset_request_writer::progress(*result.request, generation, offset, size);
  }
  return result;
}

} // namespace

struct asset_loader::implementation {
  struct job {
    std::shared_ptr<asset_request> request;
    std::uint64_t generation{};
    std::future<load_result> result;
  };
  std::vector<job> jobs;
};

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
  implementation_->jobs.push_back({.request = request,
                                   .generation = generation,
                                   .result = std::async(std::launch::async, read_asset, request,
                                                        generation, std::move(location))});
  return request;
}

void asset_loader::poll() {
  auto& jobs = implementation_->jobs;
  for (auto current = jobs.begin(); current != jobs.end();) {
    if (current->result.wait_for(std::chrono::milliseconds{0}) != std::future_status::ready) {
      ++current;
      continue;
    }
    try {
      auto result = current->result.get();
      if (!result.diagnostic.empty()) {
        static_cast<void>(asset_request_writer::fail(*result.request, result.generation,
                                                     result.error, std::move(result.diagnostic)));
      } else if (!result.bytes.empty()) {
        static_cast<void>(asset_request_writer::complete(*result.request, result.generation,
                                                         std::move(result.bytes)));
      }
    } catch (const std::bad_alloc&) {
      static_cast<void>(asset_request_writer::fail(*current->request, current->generation,
                                                   asset_request_error::out_of_memory,
                                                   "读取资产时内存不足"));
    } catch (...) {
      static_cast<void>(asset_request_writer::fail(*current->request, current->generation,
                                                   asset_request_error::io_error,
                                                   "读取资产时发生未知异常"));
    }
    current = jobs.erase(current);
  }
}

} // namespace granit::example::assets
