// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXAMPLES_COMMON_GLTF_IMAGE_DECODER_H_
#define GRANIT_EXAMPLES_COMMON_GLTF_IMAGE_DECODER_H_

#include "gltf/scene.h"

#include <cstddef>
#include <span>

namespace granit::example::gltf {

enum class image_decode_error { none, invalid_data, numeric_overflow, out_of_memory };

/** 将 PNG/JPEG 字节解码为自有 RGBA8 像素；失败时 output 保持不变。 */
[[nodiscard]] image_decode_error decode_image(std::span<const std::byte> encoded, image& output);

} // namespace granit::example::gltf

#endif
