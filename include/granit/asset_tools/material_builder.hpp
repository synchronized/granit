// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_BUILDER_HPP_
#define GRANIT_MATERIAL_BUILDER_HPP_

#include <granit/core/result.hpp>
#include <granit/asset_tools/material_builder.h>

#include <cstddef>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::asset_tools::material {

struct build_desc {
  std::string_view source_json;
  std::span<const std::string_view> shader_indices;
};

class result {
public:
  result() = default;
  ~result() { reset(); }
  result(const result&) = delete;
  result& operator=(const result&) = delete;
  result(result&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  result& operator=(result&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] std::span<const std::byte> archive() const noexcept;
  [[nodiscard]] std::string_view debug_json() const noexcept;
  [[nodiscard]] std::string_view diagnostic() const noexcept;
  void reset() noexcept;

private:
  friend std::pair<::granit::result, result> build(const build_desc&) noexcept;
  friend std::pair<::granit::result, result> inspect(std::span<const std::byte>) noexcept;

  explicit result(granit_asset_tools_material_result handle) noexcept : handle_(handle) {}

  granit_asset_tools_material_result handle_{};
};

[[nodiscard]] std::pair<::granit::result, result> build(const build_desc& desc) noexcept;
[[nodiscard]] std::pair<::granit::result, result>
inspect(std::span<const std::byte> archive) noexcept;

inline std::span<const std::byte> result::archive() const noexcept {
  const void* data = nullptr;
  uint64_t size = 0;
  if (granit_asset_tools_material_result_get_archive(handle_, &data, &size) != GRANIT_SUCCESS)
    return {};
  return {static_cast<const std::byte*>(data), static_cast<std::size_t>(size)};
}

inline std::string_view result::debug_json() const noexcept {
  const char* data = nullptr;
  uint64_t size = 0;
  if (granit_asset_tools_material_result_get_debug_json(handle_, &data, &size) != GRANIT_SUCCESS)
    return {};
  return {data, static_cast<std::size_t>(size)};
}

inline std::string_view result::diagnostic() const noexcept {
  const char* data = nullptr;
  uint64_t size = 0;
  if (granit_asset_tools_material_result_get_diagnostic(handle_, &data, &size) != GRANIT_SUCCESS)
    return {};
  return {data, static_cast<std::size_t>(size)};
}

inline void result::reset() noexcept {
  if (handle_ != 0) {
    static_cast<void>(granit_asset_tools_material_result_destroy(handle_));
    handle_ = 0;
  }
}

inline std::pair<::granit::result, result> build(const build_desc& desc) noexcept {
  try {
    std::vector<granit_asset_tools_material_shader_index> indices;
    indices.reserve(desc.shader_indices.size());
    for (const auto json : desc.shader_indices) {
      indices.push_back(
          {sizeof(granit_asset_tools_material_shader_index), 0, json.data(), json.size()});
    }
    const granit_asset_tools_material_build_desc native{
        sizeof(granit_asset_tools_material_build_desc),
        0,
        desc.source_json.data(),
        desc.source_json.size(),
        indices.data(),
        static_cast<uint32_t>(indices.size()),
        0};
    granit_asset_tools_material_result handle = 0;
    const auto status = granit_asset_tools_material_build(&native, &handle);
    return {::granit::from_native(status), result{handle}};
  } catch (const std::bad_alloc&) {
    return {::granit::result::out_of_memory, result{}};
  } catch (...) {
    return {::granit::result::internal, result{}};
  }
}

inline std::pair<::granit::result, result> inspect(std::span<const std::byte> archive) noexcept {
  granit_asset_tools_material_result handle = 0;
  const auto status = granit_asset_tools_material_inspect(archive.data(), archive.size(), &handle);
  return {::granit::from_native(status), result{handle}};
}

} // namespace granit::asset_tools::material

#endif
