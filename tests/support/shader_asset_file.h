// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_SHADER_ASSET_FILE_H_
#define GRANIT_TESTS_SUPPORT_SHADER_ASSET_FILE_H_

#include <granit/renderer/shader.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

namespace granit::tests {

inline bool read_shader_bytes(const std::filesystem::path& path, std::vector<std::byte>& output) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  output.resize(bytes.size());
  if (!bytes.empty())
    std::memcpy(output.data(), bytes.data(), bytes.size());
  return !stream.bad() && !bytes.empty();
}

// 文件读取留在测试层；只读取实际后端的 sidecar，创建时由 Core 校验摘要和能力。
class shader_asset_file {
public:
  [[nodiscard]] result load(granit_renderer renderer, const std::filesystem::path& path) {
    manifest_.clear();
    sidecar_.clear();
    granit_renderer_shader_capabilities caps = GRANIT_RENDERER_SHADER_CAPABILITIES_INIT;
    const auto status = granit_renderer_get_shader_capabilities(renderer, &caps);
    if (status != GRANIT_SUCCESS)
      return from_native(status);
    if (!read_shader_bytes(path, manifest_))
      return result::invalid_argument;
    shader_asset_info info;
    const auto inspected = inspect_shader_asset(manifest_, info);
    if (inspected.failed())
      return inspected;
    const auto suffix = caps.backend == GRANIT_RENDERER_BACKEND_VULKAN ? ".spv" : ".wgsl";
    if (!read_shader_bytes(path.string() + suffix, sidecar_))
      return result::invalid_argument;
    return result::success;
  }

  [[nodiscard]] packaged_shader_asset_desc desc() const noexcept { return {manifest_, sidecar_}; }

private:
  std::vector<std::byte> manifest_;
  std::vector<std::byte> sidecar_;
};

inline result load_shader_asset(granit_renderer renderer, const std::filesystem::path& path,
                                shader& output) {
  shader_asset_file asset;
  const auto status = asset.load(renderer, path);
  return status.failed() ? status : output.initialize_packaged_asset(renderer, asset.desc());
}

} // namespace granit::tests

#endif
