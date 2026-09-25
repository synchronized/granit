// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/asset_tools/environment_builder.hpp>

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

int main() {
  std::array<std::byte, 48> irradiance{};
  std::array<std::byte, 192> prefiltered_2x2{};
  std::array<std::byte, 48> prefiltered_1x1{};
  std::array<std::byte, 8> brdf{};
  const std::array mips{granit::asset_tools::environment::mip_desc{2, prefiltered_2x2},
                        granit::asset_tools::environment::mip_desc{1, prefiltered_1x1}};
  const granit::asset_tools::environment::build_desc desc{0.12F, -0.5F, 1, irradiance,
                                                          mips,  1,     1, brdf};
  auto [first_status, first] = granit::asset_tools::environment::build(desc);
  auto [second_status, second] = granit::asset_tools::environment::build(desc);
  const auto first_info = first.info();
  if (first_status.failed() || second_status.failed() || first_info.package.empty() ||
      first_info.debug_json.empty() || !first_info.diagnostic.empty() ||
      first.package().size() != second.package().size() ||
      !std::ranges::equal(first.package(), second.package()) ||
      first.debug_json().find("\"magic\": \"GRENV03\"") == std::string_view::npos) {
    return 1;
  }
  auto [inspect_status, inspected] = granit::asset_tools::environment::inspect(first.package());
  if (inspect_status.failed() || inspected.package().size() != first.package().size())
    return 2;
  std::vector<std::byte> corrupted(first.package().begin(), first.package().end());
  corrupted.back() = std::byte{1};
  auto [corrupt_status, corrupt_result] = granit::asset_tools::environment::inspect(corrupted);
  if (corrupt_status != granit::result::invalid_argument ||
      corrupt_result.info().diagnostic.empty() || !corrupt_result.info().package.empty())
    return 3;
  const std::array invalid_mips{granit::asset_tools::environment::mip_desc{2, prefiltered_2x2},
                                granit::asset_tools::environment::mip_desc{2, prefiltered_1x1}};
  auto invalid_desc = desc;
  invalid_desc.prefiltered_mips = invalid_mips;
  auto [invalid_status, invalid_result] = granit::asset_tools::environment::build(invalid_desc);
  if (invalid_status != granit::result::invalid_argument || invalid_result.diagnostic().empty())
    return 4;
  invalid_desc = desc;
  invalid_desc.recommended_exposure_ev = 25.0F;
  return granit::asset_tools::environment::build(invalid_desc).first ==
                 granit::result::invalid_argument
             ? 0
             : 5;
}
