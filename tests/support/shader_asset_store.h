// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_
#define GRANIT_TESTS_SUPPORT_SHADER_ASSET_STORE_H_

#include "assets/shader_asset.h"
#include "material/material_package.h"

#include <granit/pipeline/material.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace granit::tests {

class shader_asset_store {
public:
  bool add(const std::filesystem::path& manifest_path) {
    entry value;
    if (!read(manifest_path, value.manifest) ||
        !read(std::filesystem::path{manifest_path.string() + ".spv"}, value.spirv) ||
        !read(std::filesystem::path{manifest_path.string() + ".wgsl"}, value.wgsl)) {
      return false;
    }
    granit::tools::shader_asset_view view;
    if (granit::tools::decode_shader_asset(value.manifest, view) !=
        granit::tools::shader_asset_error::success) {
      return false;
    }
    value.id = view.content_id;
    entries_.push_back(std::move(value));
    return true;
  }

  [[nodiscard]] granit::material::material_shader_code
  reference(const std::filesystem::path& manifest_path) const {
    std::vector<std::byte> manifest;
    if (!read(manifest_path, manifest))
      return {};
    granit::tools::shader_asset_view view;
    if (granit::tools::decode_shader_asset(manifest, view) !=
        granit::tools::shader_asset_error::success) {
      return {};
    }
    return {.stage = view.stage == GRANIT_SHADER_STAGE_VERTEX
                         ? granit::material::package_shader_stage::vertex
                         : granit::material::package_shader_stage::fragment,
            .entry_point = std::string{view.entry_point},
            .asset_id = view.content_id};
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
      const auto* sidecar = backend == GRANIT_RENDERER_BACKEND_VULKAN
                                ? &value.spirv
                                : backend == GRANIT_RENDERER_BACKEND_WEBGPU ? &value.wgsl : nullptr;
      if (sidecar == nullptr)
        return GRANIT_ERROR_UNSUPPORTED;
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
    std::array<std::byte, 32> id{};
    std::vector<std::byte> manifest;
    std::vector<std::byte> spirv;
    std::vector<std::byte> wgsl;
  };

  static bool read(const std::filesystem::path& path, std::vector<std::byte>& output) {
    std::ifstream stream{path, std::ios::binary};
    const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
    if (bytes.empty())
      return false;
    output.resize(bytes.size());
    std::memcpy(output.data(), bytes.data(), bytes.size());
    return true;
  }

  std::vector<entry> entries_;
};

} // namespace granit::tests

#endif
