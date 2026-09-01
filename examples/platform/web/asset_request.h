// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_PLATFORM_WEB_ASSET_REQUEST_H_
#define GRANIT_EXAMPLES_PLATFORM_WEB_ASSET_REQUEST_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace granit::example::model_viewer::web {

enum class asset_request_status { idle, pending, ready, failed };

/** 保存浏览器异步资源请求结果，并拒绝来自旧 generation 的回调。 */
class asset_request {
public:
  [[nodiscard]] std::uint64_t begin(std::string url);
  [[nodiscard]] bool complete(std::uint64_t generation, std::span<const std::byte> bytes);
  [[nodiscard]] bool fail(std::uint64_t generation, std::string diagnostic);
  void cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] asset_request_status status() const noexcept { return status_; }
  [[nodiscard]] std::uint64_t generation() const noexcept { return generation_; }
  [[nodiscard]] const std::string& url() const noexcept { return url_; }
  [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
  [[nodiscard]] const std::string& diagnostic() const noexcept { return diagnostic_; }

private:
  std::uint64_t generation_{};
  asset_request_status status_{asset_request_status::idle};
  std::string url_;
  std::vector<std::byte> bytes_;
  std::string diagnostic_;
};

} // namespace granit::example::model_viewer::web

#endif
