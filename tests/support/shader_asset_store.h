// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_
#define GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_

#include "material/material_package.h"
#include "shader_asset_file.h"

#include <granit/pipeline/material.h>

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
    granit::shader_asset_info view;
    if (granit::inspect_shader_asset(value.manifest, view).failed())
      return false;
    // 测试资产允许只部署一个后端；缺少的变体由 resolver 明确返回失败。
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

  static granit_result resolve(void* user_data, const std::uint8_t asset_id[32],
                               granit_renderer_backend backend, std::uint32_t profile,
                               granit_shader_asset_desc* asset) noexcept {
    if (user_data == nullptr || asset_id == nullptr || asset == nullptr ||
        profile != GRANIT_SHADER_PROFILE_PORTABLE) {
      return GRANIT_ERROR_INVALID_ARGUMENT;
    }
    const auto& self = *static_cast<const shader_asset_store*>(user_data);
    for (const auto& value : self.entries_) {
      if (std::memcmp(value.id.data(), asset_id, value.id.size()) != 0)
        continue;
      const auto* sidecar = backend == GRANIT_RENDERER_BACKEND_VULKAN   ? &value.spirv
                            : backend == GRANIT_RENDERER_BACKEND_WEBGPU ? &value.wgsl
                                                                        : nullptr;
      if (sidecar == nullptr)
        return GRANIT_ERROR_UNSUPPORTED;
      if (sidecar->empty())
        return GRANIT_ERROR_NOT_READY;
      *asset = GRANIT_SHADER_ASSET_DESC_INIT;
      asset->manifest_data = value.manifest.data();
      asset->manifest_size = value.manifest.size();
      asset->sidecar_data = sidecar->data();
      asset->sidecar_size = sidecar->size();
      return GRANIT_SUCCESS;
    }
    return GRANIT_ERROR_NOT_READY;
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
