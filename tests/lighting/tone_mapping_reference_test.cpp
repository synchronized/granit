// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "reference/lighting/tone_mapping_reference.h"

#include <catch2/catch_all.hpp>

#include <limits>

TEST_CASE("Tone Mapping输出传递方式必须匹配Attachment格式") {
  using namespace granit::lighting;
  CHECK(validate_tone_mapping_output(granit::texture_format::rgba8_srgb,
                                     tone_mapping_output_transfer::attachment_srgb) ==
        tone_mapping_error::none);
  CHECK(validate_tone_mapping_output(granit::texture_format::rgba8_unorm,
                                     tone_mapping_output_transfer::shader_srgb) ==
        tone_mapping_error::none);
  CHECK(validate_tone_mapping_output(granit::texture_format::rgba8_unorm,
                                     tone_mapping_output_transfer::attachment_srgb) ==
        tone_mapping_error::incompatible_output);
  CHECK(validate_tone_mapping_output(granit::texture_format::rgba8_srgb,
                                     tone_mapping_output_transfer::shader_srgb) ==
        tone_mapping_error::incompatible_output);
}

TEST_CASE("ACES fitted覆盖黑色负值和高亮") {
  CHECK(granit::lighting::aces_fitted(0.0F) == 0.0F);
  CHECK(granit::lighting::aces_fitted(-1.0F) == 0.0F);
  CHECK(granit::lighting::aces_fitted(1.0F) == Catch::Approx(0.803797F));
  CHECK(granit::lighting::aces_fitted(100.0F) == 1.0F);
}

TEST_CASE("线性到sRGB使用标准分段转换") {
  CHECK(granit::lighting::linear_to_srgb(0.0F) == 0.0F);
  CHECK(granit::lighting::linear_to_srgb(0.0031308F) == Catch::Approx(0.0404499F));
  CHECK(granit::lighting::linear_to_srgb(0.5F) == Catch::Approx(0.735357F));
  CHECK(granit::lighting::linear_to_srgb(1.0F) == 1.0F);
}

TEST_CASE("曝光EV在ACES前以二次幂缩放HDR") {
  granit::math::float3 output{};
  CHECK(granit::lighting::evaluate_tone_mapping(
            {0.5F, 1.0F, 2.0F}, {.exposure_ev = 1.0F}, output) ==
        granit::lighting::tone_mapping_error::none);
  CHECK(output.x == Catch::Approx(granit::lighting::aces_fitted(1.0F)));
  CHECK(output.y == Catch::Approx(granit::lighting::aces_fitted(2.0F)));
  CHECK(output.z == Catch::Approx(granit::lighting::aces_fitted(4.0F)));
}

TEST_CASE("非法Tone Mapping输入不修改输出") {
  granit::math::float3 output{1.0F, 2.0F, 3.0F};
  CHECK(granit::lighting::evaluate_tone_mapping(
            {std::numeric_limits<float>::infinity(), 0.0F, 0.0F}, {}, output) ==
        granit::lighting::tone_mapping_error::invalid_color);
  CHECK(output == granit::math::float3{1.0F, 2.0F, 3.0F});
  CHECK(granit::lighting::evaluate_tone_mapping(
            {}, {.exposure_ev = 25.0F}, output) ==
        granit::lighting::tone_mapping_error::invalid_exposure);
  CHECK(output == granit::math::float3{1.0F, 2.0F, 3.0F});
}
