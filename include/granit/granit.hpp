// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_GRANIT_HPP_
#define GRANIT_GRANIT_HPP_

#include <cstdint>

#include <granit/core/diagnostic.hpp>
#include <granit/core/result.hpp>
#include <granit/core/types.hpp>
#include <granit/granit.h>
#include <granit/math/types.hpp>
#include <granit/renderer/buffer.hpp>
#include <granit/renderer/command_recorder.hpp>
#include <granit/renderer/pipeline.hpp>
#include <granit/renderer/render_target.hpp>
#include <granit/renderer/renderer.hpp>
#include <granit/renderer/resource_types.hpp>
#include <granit/renderer/sampler.hpp>
#include <granit/renderer/shader.hpp>
#include <granit/renderer/surface.hpp>
#include <granit/renderer/swapchain.hpp>
#include <granit/renderer/texture.hpp>
#include <granit/renderer/timestamp_query.hpp>
#include <granit/renderer/upload_batch.hpp>

namespace granit {

struct version {
  std::uint32_t major;
  std::uint32_t minor;
  std::uint32_t patch;
};

[[nodiscard]] inline version library_version() noexcept {
  return {granit_version_major(), granit_version_minor(), granit_version_patch()};
}

} // namespace granit

#endif
