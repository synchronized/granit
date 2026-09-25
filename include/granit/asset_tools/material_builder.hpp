// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_MATERIAL_BUILDER_HPP_
#define GRANIT_MATERIAL_BUILDER_HPP_

#include <granit/asset_tools/material_builder.h>
#include <granit/core/result.hpp>

#include <cstddef>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::asset_tools::material {

struct build_desc {
  std::string_view source_json;
  std::span<const std::span<const std::byte>> shader_libraries;
};

struct result_info {
  std::span<const std::byte> archive;
  std::string_view debug_json;
  std::string_view diagnostic;
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
  [[nodiscard]] result_info info() const noexcept;
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

inline std::span<const std::byte> result::archive() const noexcept { return info().archive; }

inline std::string_view result::debug_json() const noexcept { return info().debug_json; }

inline std::string_view result::diagnostic() const noexcept { return info().diagnostic; }

inline result_info result::info() const noexcept {
  granit_asset_tools_material_result_info value = GRANIT_ASSET_TOOLS_MATERIAL_RESULT_INFO_INIT;
  if (granit_asset_tools_material_result_get_info(handle_, &value) != GRANIT_SUCCESS)
    return {};
  return {.archive = {static_cast<const std::byte*>(value.archive),
                      static_cast<std::size_t>(value.archive_size)},
          .debug_json = {value.debug_json, static_cast<std::size_t>(value.debug_json_length)},
          .diagnostic = {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
}

inline void result::reset() noexcept {
  if (handle_ != 0) {
    static_cast<void>(granit_asset_tools_material_result_destroy(handle_));
    handle_ = 0;
  }
}

inline std::pair<::granit::result, result> build(const build_desc& desc) noexcept {
  try {
    std::vector<granit_asset_tools_material_shader_library> libraries;
    libraries.reserve(desc.shader_libraries.size());
    for (const auto archive : desc.shader_libraries) {
      libraries.push_back(
          {sizeof(granit_asset_tools_material_shader_library), 0, archive.data(), archive.size()});
    }
    const granit_asset_tools_material_build_desc native{
        sizeof(granit_asset_tools_material_build_desc),
        0,
        desc.source_json.data(),
        desc.source_json.size(),
        libraries.data(),
        static_cast<uint32_t>(libraries.size()),
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
