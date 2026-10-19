// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/asset_tools.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

int main() {
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
             : 7;
}
