// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_SUPPORT_TONE_MAPPING_SHADER_LIBRARY_H_
#define GRANIT_TESTS_SUPPORT_TONE_MAPPING_SHADER_LIBRARY_H_

#include "shader_asset_store.h"

#include <array>
#include <cstddef>
#include <string>
#include <vector>

namespace granit::tests {

/** 从测试构建目录的中间资产组装 Tone Mapping Library，并持有其借用字节。 */
class tone_mapping_shader_library {
public:
  [[nodiscard]] bool initialize(granit_renderer renderer) {
    const auto vertex_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.vert.grshader";
    const auto fragment_path =
        std::string{GRANIT_PIPELINE_SHADER_DIR} + "/tone_mapping.frag.grshader";
    if (!assets_.add(vertex_path) || !assets_.add(fragment_path) ||
        !assets_.initialize_library(renderer, bytes_, library_)) {
      return false;
    }
    vertex_id_ = assets_.reference(vertex_path).asset_id;
    fragment_id_ = assets_.reference(fragment_path).asset_id;
    return true;
  }

  [[nodiscard]] granit_shader_library native_handle() const noexcept {
    return library_.native_handle();
  }
  [[nodiscard]] const std::array<std::byte, GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE>&
  vertex_id() const noexcept {
    return vertex_id_;
  }
  [[nodiscard]] const std::array<std::byte, GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE>&
  fragment_id() const noexcept {
    return fragment_id_;
  }

private:
  shader_asset_store assets_;
  std::vector<std::byte> bytes_;
  granit::shader_library library_;
  std::array<std::byte, GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE> vertex_id_{};
  std::array<std::byte, GRANIT_SHADER_LIBRARY_CONTENT_DIGEST_SIZE> fragment_id_{};
};

} // namespace granit::tests

#endif
