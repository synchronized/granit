// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ENVIRONMENT_BUILDER_HPP_
#define GRANIT_ENVIRONMENT_BUILDER_HPP_

#include <granit/asset_tools/environment_builder.h>
#include <granit/core/result.hpp>

#include <cstddef>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::asset_tools::environment {

struct build_desc;

struct result_info {
  std::span<const std::byte> package;
  std::string_view debug_json;
  std::string_view diagnostic;
};

class result final {
public:
  result() noexcept = default;
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
  explicit operator bool() const noexcept { return handle_ != 0; }
  [[nodiscard]] result_info info() const noexcept {
    granit_asset_tools_environment_result_info value =
        GRANIT_ASSET_TOOLS_ENVIRONMENT_RESULT_INFO_INIT;
    if (granit_asset_tools_environment_result_get_info(handle_, &value) != GRANIT_SUCCESS)
      return {};
    return {.package = {static_cast<const std::byte*>(value.package),
                        static_cast<std::size_t>(value.package_size)},
            .debug_json = {value.debug_json, static_cast<std::size_t>(value.debug_json_length)},
            .diagnostic = {value.diagnostic, static_cast<std::size_t>(value.diagnostic_length)}};
  }
  [[nodiscard]] std::span<const std::byte> package() const noexcept { return info().package; }
  [[nodiscard]] std::string_view debug_json() const noexcept { return info().debug_json; }
  [[nodiscard]] std::string_view diagnostic() const noexcept { return info().diagnostic; }
  void reset() noexcept {
    if (handle_ != 0)
      static_cast<void>(granit_asset_tools_environment_result_destroy(std::exchange(handle_, 0)));
  }

private:
  friend std::pair<granit::result, result> build(const build_desc&) noexcept;
  friend std::pair<granit::result, result> inspect(std::span<const std::byte>) noexcept;

  explicit result(granit_asset_tools_environment_result handle) noexcept : handle_(handle) {}

  granit_asset_tools_environment_result handle_{};
};

struct mip_desc {
  std::uint32_t resolution{};
  std::span<const std::byte> pixels;
};

struct build_desc {
  float recommended_environment_intensity{0.12F};
  float recommended_exposure_ev{-0.5F};
  std::uint32_t irradiance_resolution{};
  std::span<const std::byte> irradiance_pixels;
  std::span<const mip_desc> prefiltered_mips;
  std::uint32_t brdf_width{};
  std::uint32_t brdf_height{};
  std::span<const std::byte> brdf_pixels;
};

[[nodiscard]] inline std::pair<granit::result, result> build(const build_desc& desc) noexcept {
  if (desc.prefiltered_mips.size() > UINT32_MAX)
    return {granit::result::invalid_argument, result{}};
  try {
    std::vector<granit_asset_tools_environment_mip_desc> mips;
    mips.reserve(desc.prefiltered_mips.size());
    for (const auto& mip : desc.prefiltered_mips) {
      mips.push_back({sizeof(granit_asset_tools_environment_mip_desc),
                      mip.resolution,
                      mip.pixels.data(),
                      mip.pixels.size(),
                      {0, 0}});
    }
    const granit_asset_tools_environment_build_desc native{
        sizeof(granit_asset_tools_environment_build_desc),
        desc.recommended_environment_intensity,
        desc.recommended_exposure_ev,
        desc.irradiance_resolution,
        desc.irradiance_pixels.data(),
        desc.irradiance_pixels.size(),
        mips.data(),
        static_cast<std::uint32_t>(mips.size()),
        desc.brdf_width,
        desc.brdf_height,
        desc.brdf_pixels.data(),
        desc.brdf_pixels.size(),
        {0, 0}};
    granit_asset_tools_environment_result handle = 0;
    const auto status = granit_asset_tools_environment_build(&native, &handle);
    return {from_native(status), result{handle}};
  } catch (const std::bad_alloc&) {
    return {granit::result::out_of_memory, result{}};
  } catch (...) {
    return {granit::result::internal, result{}};
  }
}

[[nodiscard]] inline std::pair<granit::result, result>
inspect(std::span<const std::byte> package) noexcept {
  granit_asset_tools_environment_result handle = 0;
  const auto status =
      granit_asset_tools_environment_inspect(package.data(), package.size(), &handle);
  return {from_native(status), result{handle}};
}

} // namespace granit::asset_tools::environment

#endif
