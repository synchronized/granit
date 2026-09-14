// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_ASSET_TOOLS_SHADER_LIBRARY_SOURCE_MANIFEST_H_
#define GRANIT_ASSET_TOOLS_SHADER_LIBRARY_SOURCE_MANIFEST_H_

#include <granit/core/content_id.hpp>
#include <granit/core/shader_types.hpp>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace granit::asset_tools::detail {

enum class shader_library_source_error {
  none,
  invalid_argument,
  invalid_json,
  invalid_schema,
  unsupported_version,
  out_of_memory,
  internal,
};

struct shader_library_source_define {
  std::string name;
  std::string value;
};

struct shader_library_source_variant {
  std::string name;
  std::vector<shader_library_source_define> defines;
};

struct shader_library_source_shader {
  std::string name;
  std::string source;
  shader_stage stage{shader_stage::vertex};
  std::string entry_point;
  std::vector<shader_library_source_variant> variants;
};

struct shader_library_source_manifest {
  std::string name;
  shader_backend target_backends{shader_backend::all};
  shader_profile target_profile{shader_profile::portable};
  std::vector<shader_library_source_shader> shaders;
};

struct shader_library_index_entry {
  std::string name;
  shader_content_id content_id{};
  shader_stage stage{shader_stage::vertex};
  std::string entry_point;
};

struct shader_library_index {
  std::string library;
  content_digest library_digest{};
  std::vector<shader_library_index_entry> shaders;
};

[[nodiscard]] shader_library_source_error
parse_shader_library_source_manifest(std::string_view json,
                                     shader_library_source_manifest& manifest) noexcept;

[[nodiscard]] shader_library_source_error
encode_shader_library_index_json(const shader_library_index& index, std::string& json) noexcept;

[[nodiscard]] shader_library_source_error
parse_shader_library_index_json(std::string_view json, shader_library_index& index) noexcept;

} // namespace granit::asset_tools::detail

#endif
