// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/tools/shader_library_builder.hpp>

#include <array>
#include <cstring>
#include <filesystem>
#include <tuple>

int main(int argc, char** argv) {
  if (argc != 4)
    return 1;
  const std::array objects{
      granit_shader_tools_library_object_desc{sizeof(granit_shader_tools_library_object_desc), 0,
                                              argv[1], std::strlen(argv[1])},
      granit_shader_tools_library_object_desc{sizeof(granit_shader_tools_library_object_desc), 0,
                                              argv[2], std::strlen(argv[2])},
  };
  const auto output = (std::filesystem::path{argv[3]} / "fixture.grshlib").string();
  std::error_code error;
  std::filesystem::remove_all(argv[3], error);
  granit_shader_tools_library_desc desc{
      .struct_size = sizeof(granit_shader_tools_library_desc),
      .reserved = 0,
      .objects = objects.data(),
      .object_count = objects.size(),
      .target_backends = GRANIT_SHADER_BACKEND_ALL_BITS,
      .reserved2 = 0,
      .output_path = output.data(),
      .output_path_length = output.size(),
  };
  auto [status, cache_hit] = granit::shader_tools::build_library(desc);
  if (status.failed() || cache_hit || !std::filesystem::exists(output))
    return 2;
  std::tie(status, cache_hit) = granit::shader_tools::build_library(desc);
  if (status.failed() || !cache_hit)
    return 3;
  desc.target_backends = 0;
  std::tie(status, cache_hit) = granit::shader_tools::build_library(desc);
  if (status != granit::result::invalid_argument || cache_hit)
    return 4;
  desc.target_backends = GRANIT_SHADER_BACKEND_VULKAN_BIT;
  std::tie(status, cache_hit) = granit::shader_tools::build_library(desc);
  if (status.failed() || cache_hit)
    return 5;
  std::filesystem::remove_all(argv[3], error);
  return 0;
}
