// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/asset_tools.hpp>

#include <array>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

namespace {

std::string read_text(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  return {std::istreambuf_iterator<char>{stream}, {}};
}

std::vector<std::byte> read_bytes(const std::filesystem::path& path) {
  std::ifstream stream{path, std::ios::binary};
  const std::vector<char> bytes{std::istreambuf_iterator<char>{stream}, {}};
  std::vector<std::byte> result(bytes.size());
  std::ranges::copy(std::as_bytes(std::span{bytes}), result.begin());
  return result;
}

} // namespace

int main(int argc, char** argv) {
  const auto [status, capabilities] = granit::asset_tools::shader::target_capabilities(
      granit::shader_backend::vulkan, granit::shader_profile::portable);
  static_cast<void>(capabilities);
  if (status != granit::result::success)
    return 1;

  constexpr std::array<std::byte, 64> texture_payload{};
  const granit::asset_tools::texture::subresource_info subresource{
      0, 0, 0, texture_payload.size(), 16, 4};
  const granit::asset_tools::texture::variant_desc variant{
      granit::texture_format::rgba8_srgb,
      granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
      texture_payload,
      std::span{&subresource, 1}};
  const granit::asset_tools::texture::build_desc texture_desc{
      granit::texture_dimension::two_dimensional, 4, 4, 1, 1, 1, std::span{&variant, 1}};
  auto [texture_status, texture] = granit::asset_tools::texture::build(texture_desc);
  if (texture_status.failed() || !texture || texture.manifest().empty() ||
      texture.payload().size() != texture_payload.size())
    return 2;
  auto [texture_inspect_status, inspected_texture] =
      granit::asset_tools::texture::inspect(texture.manifest());
  if (texture_inspect_status.failed() || !inspected_texture ||
      !inspected_texture.payload().empty())
    return 3;

  std::array<std::byte, 48> irradiance{};
  std::array<std::byte, 192> prefiltered_2x2{};
  std::array<std::byte, 48> prefiltered_1x1{};
  std::array<std::byte, 8> brdf{};
  const std::array mips{
      granit::asset_tools::environment::mip_desc{2, prefiltered_2x2},
      granit::asset_tools::environment::mip_desc{1, prefiltered_1x1}};
  const granit::asset_tools::environment::build_desc environment_desc{
      0.12F, -0.5F, 1, irradiance, mips, 1, 1, brdf};
  auto [environment_status, environment] =
      granit::asset_tools::environment::build(environment_desc);
  if (environment_status.failed() || !environment || environment.package().empty())
    return 4;
  auto [environment_inspect_status, inspected_environment] =
      granit::asset_tools::environment::inspect(environment.package());
  if (environment_inspect_status.failed() || !inspected_environment ||
      inspected_environment.package().size() != environment.package().size())
    return 5;

  std::vector<std::byte> corrupted(environment.package().begin(), environment.package().end());
  corrupted.back() = std::byte{1};
  auto [corrupt_status, corrupt_environment] =
      granit::asset_tools::environment::inspect(corrupted);
  if (corrupt_status != granit::result::invalid_argument ||
      corrupt_environment.diagnostic().empty() || !corrupt_environment.package().empty())
    return 6;

  if (argc != 3)
    return 7;
  const auto material_source = read_text(argv[1]);
  const auto shader_library = read_bytes(argv[2]);
  const granit::asset_tools::material::build_desc::shader_library libraries[]{{shader_library}};
  auto [material_status, material] = granit::asset_tools::material::build(
      {.source_json = material_source, .shader_libraries = libraries});
  if (material_status.failed() || !material || material.archive().empty() ||
      material.debug_json().find("\"magic\": \"GRMAT\"") == std::string_view::npos)
    return 8;
  auto [material_inspect_status, inspected_material] =
      granit::asset_tools::material::inspect(material.archive());
  if (material_inspect_status.failed() || !inspected_material ||
      inspected_material.archive().size() != material.archive().size())
    return 9;

  const std::array<std::byte, 1> invalid_archive{std::byte{0}};
  const granit::asset_tools::material::build_desc::shader_library invalid_libraries[]{
      {invalid_archive}};
  auto [invalid_material_status, invalid_material] = granit::asset_tools::material::build(
      {.source_json = material_source, .shader_libraries = invalid_libraries});
  if (invalid_material_status != granit::result::invalid_argument || !invalid_material ||
      invalid_material.diagnostic().empty() || !invalid_material.archive().empty())
    return 10;

  if (argc == 7) {
    const granit::asset_tools::shader::source_library_desc library_desc{
        argv[3], argv[4], argv[5], argv[6]};
    auto [library_status, library] =
        granit::asset_tools::shader::build_library_from_manifest(library_desc);
    if (library_status.failed() || !library || !library.info().diagnostic.empty() ||
        !std::filesystem::exists(argv[6]))
      return 11;
  } else if (argc != 3) {
    return 12;
  }

  const granit::asset_tools::texture::subresource_info invalid_subresource{
      0, 0, 0, texture_payload.size() - 1, 16, 4};
  const granit::asset_tools::texture::variant_desc invalid_variant{
      granit::texture_format::rgba8_srgb,
      granit::texture_usage::sampled | granit::texture_usage::transfer_destination,
      texture_payload,
      std::span{&invalid_subresource, 1}};
  const granit::asset_tools::texture::build_desc invalid_texture_desc{
      granit::texture_dimension::two_dimensional, 4, 4, 1, 1, 1,
      std::span{&invalid_variant, 1}};
  auto [invalid_status, invalid_texture] =
      granit::asset_tools::texture::build(invalid_texture_desc);
  return invalid_status == granit::result::invalid_argument && invalid_texture &&
                 invalid_texture.manifest().empty() && !invalid_texture.diagnostic().empty()
             ? 0
             : 13;
}
