// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEXTURE_BUILDER_HPP_
#define GRANIT_TEXTURE_BUILDER_HPP_

#include <granit/core/result.hpp>
#include <granit/tools/texture_builder.h>

#include <cstddef>
#include <new>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace granit::asset_tools::texture {

class result final {
public:
  result() noexcept = default;
  explicit result(granit_asset_tools_texture_result handle) noexcept : handle_(handle) {}
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
  [[nodiscard]] std::span<const std::byte> manifest() const noexcept {
    const void* data = nullptr;
    std::uint64_t size = 0;
    if (granit_asset_tools_texture_result_get_manifest(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {static_cast<const std::byte*>(data), static_cast<std::size_t>(size)};
  }
  [[nodiscard]] std::span<const std::byte> payload() const noexcept {
    const void* data = nullptr;
    std::uint64_t size = 0;
    if (granit_asset_tools_texture_result_get_payload(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {static_cast<const std::byte*>(data), static_cast<std::size_t>(size)};
  }
  [[nodiscard]] std::string_view debug_json() const noexcept {
    const char* data = nullptr;
    std::uint64_t size = 0;
    if (granit_asset_tools_texture_result_get_debug_json(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {data, static_cast<std::size_t>(size)};
  }
  [[nodiscard]] std::string_view diagnostic() const noexcept {
    const char* data = nullptr;
    std::uint64_t size = 0;
    if (granit_asset_tools_texture_result_get_diagnostic(handle_, &data, &size) != GRANIT_SUCCESS)
      return {};
    return {data, static_cast<std::size_t>(size)};
  }
  void reset() noexcept {
    if (handle_ != 0)
      static_cast<void>(granit_asset_tools_texture_result_destroy(std::exchange(handle_, 0)));
  }

private:
  granit_asset_tools_texture_result handle_{};
};

struct variant_desc {
  granit_texture_format format{};
  granit_texture_usage usage{};
  std::span<const std::byte> payload;
  std::span<const granit_texture_asset_subresource_info> subresources;
};

struct build_desc {
  granit_texture_dimension dimension{GRANIT_TEXTURE_DIMENSION_2D};
  std::uint32_t width{1};
  std::uint32_t height{1};
  std::uint32_t depth{1};
  std::uint32_t array_layers{1};
  std::uint32_t mip_levels{1};
  std::span<const variant_desc> variants;
};

[[nodiscard]] inline std::pair<granit::result, result> build(const build_desc& desc) noexcept {
  if (desc.variants.size() > UINT32_MAX)
    return {granit::result::invalid_argument, result{}};
  try {
    std::vector<granit_asset_tools_texture_variant_desc> variants;
    variants.reserve(desc.variants.size());
    for (const auto& variant : desc.variants) {
      if (variant.subresources.size() > UINT32_MAX)
        return {granit::result::invalid_argument, result{}};
      variants.push_back({sizeof(granit_asset_tools_texture_variant_desc), variant.format,
                          variant.usage, 0, variant.payload.data(), variant.payload.size(),
                          variant.subresources.data(),
                          static_cast<std::uint32_t>(variant.subresources.size()), 0});
    }
    const granit_asset_tools_texture_build_desc native{
        sizeof(granit_asset_tools_texture_build_desc),
        desc.dimension,
        desc.width,
        desc.height,
        desc.depth,
        desc.array_layers,
        desc.mip_levels,
        static_cast<std::uint32_t>(variants.size()),
        variants.data(),
        {0, 0}};
    granit_asset_tools_texture_result handle = 0;
    const auto status = granit_asset_tools_texture_build(&native, &handle);
    return {from_native(status), result{handle}};
  } catch (const std::bad_alloc&) {
    return {granit::result::out_of_memory, result{}};
  } catch (...) {
    return {granit::result::internal, result{}};
  }
}

[[nodiscard]] inline std::pair<granit::result, result>
inspect(std::span<const std::byte> manifest) noexcept {
  granit_asset_tools_texture_result handle = 0;
  const auto status = granit_asset_tools_texture_inspect(manifest.data(), manifest.size(), &handle);
  return {from_native(status), result{handle}};
}

} // namespace granit::asset_tools::texture

#endif
