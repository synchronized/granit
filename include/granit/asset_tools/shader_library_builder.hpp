// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_LIBRARY_BUILDER_HPP_
#define GRANIT_SHADER_LIBRARY_BUILDER_HPP_

#include <granit/asset_tools/shader_library_builder.h>
#include <granit/core/result.hpp>
#include <string_view>
#include <utility>

namespace granit::asset_tools::shader {

struct source_library_desc {
  std::string_view manifest_path;
  std::string_view toolchain_root;
  std::string_view cache_path;
  std::string_view output_path;
};

struct library_result_info {
  bool cache_hit{};
  std::string_view failed_shader;
  std::string_view diagnostic;
};

class library_result {
public:
  library_result() = default;
  ~library_result() { reset(); }
  library_result(const library_result&) = delete;
  library_result& operator=(const library_result&) = delete;
  library_result(library_result&& other) noexcept : handle_(std::exchange(other.handle_, 0)) {}
  library_result& operator=(library_result&& other) noexcept {
    if (this != &other) {
      reset();
      handle_ = std::exchange(other.handle_, 0);
    }
    return *this;
  }

  [[nodiscard]] explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] library_result_info info() const noexcept {
    granit_asset_tools_shader_library_result_info value =
        GRANIT_ASSET_TOOLS_SHADER_LIBRARY_RESULT_INFO_INIT;
    if (granit_asset_tools_shader_library_result_get_info(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {.cache_hit = value.cache_hit != 0,
            .failed_shader = {value.failed_shader,
                              static_cast<std::size_t>(value.failed_shader_length)},
            .diagnostic = {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  void reset() noexcept {
    if (handle_ != 0) {
      static_cast<void>(granit_asset_tools_shader_library_result_destroy(handle_));
      handle_ = 0;
    }
  }

private:
  friend std::pair<::granit::result, library_result>
  build_library_from_manifest(const source_library_desc&) noexcept;
  explicit library_result(granit_asset_tools_shader_library_result handle) noexcept
      : handle_(handle) {}

  granit_asset_tools_shader_library_result handle_{};
};

inline std::pair<::granit::result, library_result>
build_library_from_manifest(const source_library_desc& desc) noexcept {
  const granit_asset_tools_shader_source_library_desc native{
      .struct_size = sizeof(granit_asset_tools_shader_source_library_desc),
      .reserved = 0,
      .manifest_path = desc.manifest_path.data(),
      .manifest_path_length = desc.manifest_path.size(),
      .toolchain_root = desc.toolchain_root.data(),
      .toolchain_root_length = desc.toolchain_root.size(),
      .cache_path = desc.cache_path.data(),
      .cache_path_length = desc.cache_path.size(),
      .output_path = desc.output_path.data(),
      .output_path_length = desc.output_path.size(),
  };
  granit_asset_tools_shader_library_result handle = 0;
  const auto status = granit_asset_tools_shader_build_library_from_manifest(&native, &handle);
  return {::granit::from_native(status), library_result{handle}};
}

} // namespace granit::asset_tools::shader

#endif
