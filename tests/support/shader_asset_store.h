// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_
#define GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_

#include "asset_formats/material/material_package.h"
#include "shader_asset_file.h"
#include "asset_formats/shader/shader_library.h"

#include <granit/renderer/shader_library.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace granit::tests {

class shader_asset_store {
public:
  bool add(const std::filesystem::path& manifest_path) {
    for (const auto& value : entries_) {
      if (value.path == manifest_path)
        return true;
    }
    entry value;
    if (!read_shader_bytes(manifest_path, value.manifest))
      return false;
    granit::detail::shader_format::shader_object_view view;
    if (granit::detail::shader_format::decode_shader_object(value.manifest, view) !=
        granit::detail::shader_format::shader_object_error::success)
      return false;
    // 测试资产允许只部署一个后端；Library 构建时由目标掩码检查缺失载荷。
    const bool spirv = read_shader_bytes(manifest_path.string() + ".spv", value.spirv);
    const bool wgsl = read_shader_bytes(manifest_path.string() + ".wgsl", value.wgsl);
    if (!spirv && !wgsl)
      return false;
    value.id = view.content_id;
    value.path = manifest_path;
    value.stage = view.stage;
    value.entry_point = std::move(view.entry_point);
    entries_.push_back(std::move(value));
    return true;
  }

  [[nodiscard]] granit::material::material_shader_code
  reference(const std::filesystem::path& manifest_path) const {
    for (const auto& value : entries_) {
      if (value.path == manifest_path) {
        if (value.stage != granit::shader_stage::vertex &&
            value.stage != granit::shader_stage::fragment)
          return {};
        return {.stage = value.stage == granit::shader_stage::vertex
                             ? granit::material::package_shader_stage::vertex
                             : granit::material::package_shader_stage::fragment,
                .entry_point = value.entry_point,
                .asset_id = value.id,
                .spirv = {},
                .wgsl = {}};
      }
    }
    return {};
  }

  bool
  build_library(std::vector<std::byte>& output,
                granit_shader_backend_flags backend_flags = GRANIT_SHADER_BACKEND_ALL_BITS) const {
    std::vector<granit::detail::shader_format::shader_library_object_source> sources;
    try {
      sources.reserve(entries_.size());
      for (const auto& value : entries_)
        sources.push_back({value.manifest, value.wgsl, value.spirv});
    } catch (...) {
      return false;
    }
    return granit::detail::shader_format::encode_shader_library({sources, backend_flags}, output) ==
           granit::detail::shader_format::shader_library_error::success;
  }

  bool initialize_library(granit_renderer renderer, std::vector<std::byte>& bytes,
                          granit::shader_library& library) const {
    return build_library(bytes) && library.initialize(renderer, bytes).ok();
  }

  bool initialize_library(granit::renderer& renderer, std::vector<std::byte>& bytes,
                          granit::shader_library& library) const {
    return build_library(bytes) && library.initialize(renderer, bytes).ok();
  }

private:
  struct entry {
    std::filesystem::path path;
    granit::shader_stage stage{};
    std::string entry_point;
    std::array<std::byte, 32> id{};
    std::vector<std::byte> manifest;
    std::vector<std::byte> spirv;
    std::vector<std::byte> wgsl;
  };

  std::vector<entry> entries_;
};

} // namespace granit::tests

#endif
