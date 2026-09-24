// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_REQUEST_H_
#define GRANIT_EXAMPLES_COMMON_ASSETS_ASSET_REQUEST_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace granit::example::assets {

enum class asset_request_status { idle, pending, ready, failed, cancelled };
enum class asset_request_error {
  none,
  invalid_location,
  io_error,
  transport_error,
  out_of_memory,
  cancelled,
};

struct asset_request_progress {
  std::uint64_t received_bytes{};
  std::optional<std::uint64_t> total_bytes;
};

/** 保存一次异步资产读取的状态、进度和自有结果。 */
class asset_request {
public:
  asset_request() = default;
  asset_request(const asset_request&) = delete;
  asset_request& operator=(const asset_request&) = delete;

  void cancel() noexcept;
  void reset() noexcept;

  [[nodiscard]] asset_request_status status() const noexcept {
    return status_.load(std::memory_order_acquire);
  }
  [[nodiscard]] asset_request_progress progress() const noexcept;
  [[nodiscard]] asset_request_error error() const noexcept { return error_; }
  [[nodiscard]] std::string_view location() const noexcept { return location_; }
  [[nodiscard]] const std::vector<std::byte>& bytes() const noexcept { return bytes_; }
  [[nodiscard]] std::string_view diagnostic() const noexcept { return diagnostic_; }

private:
  friend class asset_request_writer;

  static constexpr std::uint64_t unknown_size = std::numeric_limits<std::uint64_t>::max();

  std::atomic<std::uint64_t> generation_{};
  std::atomic<asset_request_status> status_{asset_request_status::idle};
  std::atomic<std::uint64_t> received_bytes_{};
  std::atomic<std::uint64_t> total_bytes_{unknown_size};
  asset_request_error error_{asset_request_error::none};
  std::string location_;
  std::vector<std::byte> bytes_;
  std::string diagnostic_;
};

/** 仅供平台 loader 实现推进请求；sample 不应直接调用。 */
class asset_request_writer {
public:
  [[nodiscard]] static std::uint64_t begin(asset_request& request, std::string location);
  [[nodiscard]] static bool active(const asset_request& request, std::uint64_t generation) noexcept;
  static void progress(asset_request& request, std::uint64_t generation,
                       std::uint64_t received_bytes,
                       std::optional<std::uint64_t> total_bytes = std::nullopt) noexcept;
  [[nodiscard]] static bool complete(asset_request& request, std::uint64_t generation,
                                     std::span<const std::byte> bytes);
  [[nodiscard]] static bool complete(asset_request& request, std::uint64_t generation,
                                     std::vector<std::byte>&& bytes);
  [[nodiscard]] static bool fail(asset_request& request, std::uint64_t generation,
                                 asset_request_error error, std::string diagnostic);
};

} // namespace granit::example::assets

#endif
