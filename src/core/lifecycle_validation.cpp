// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/lifecycle_validation.h"

#include <array>
#include <cinttypes>
#include <cstdio>

namespace granit::detail {
namespace {

constexpr std::array<const char*, static_cast<std::size_t>(lifecycle_resource_type::count)>
    resource_names{"Buffer", "Texture", "TextureView", "Sampler", "Surface", "Swapchain"};

constexpr std::size_t to_index(lifecycle_resource_type type) noexcept {
  return static_cast<std::size_t>(type);
}

} // namespace

void lifecycle_snapshot::add(lifecycle_resource_type type, granit_handle handle,
                             std::uint64_t creation_sequence) noexcept {
  auto& resource = summaries_[to_index(type)];
  ++resource.count;
  ++total_count_;
  if (resource.sample_count < resource.samples.size()) {
    resource.samples[resource.sample_count++] = {handle, creation_sequence};
  }
}

const lifecycle_resource_summary&
lifecycle_snapshot::summary(lifecycle_resource_type type) const noexcept {
  return summaries_[to_index(type)];
}

void write_lifecycle_diagnostic(granit_renderer renderer, std::uint32_t domain,
                                const lifecycle_snapshot& snapshot) noexcept {
  if (snapshot.empty()) {
    return;
  }

  std::fprintf(stderr,
               "[granit][validation] Renderer 0x%016" PRIx64 " (domain=%" PRIu32
               ") 销毁时仍有 %" PRIu64 " 个用户资源：",
               renderer, domain, snapshot.total_count());
  bool first = true;
  for (std::size_t index = 0; index < resource_names.size(); ++index) {
    const auto type = static_cast<lifecycle_resource_type>(index);
    const auto& resource = snapshot.summary(type);
    if (resource.count == 0) {
      continue;
    }
    std::fprintf(stderr, "%s%s=%" PRIu64, first ? "" : ", ", resource_names[index], resource.count);
    first = false;
    if (resource.sample_count != 0) {
      std::fputs(" [", stderr);
      for (std::size_t sample = 0; sample < resource.sample_count; ++sample) {
        const auto& item = resource.samples[sample];
        std::fprintf(stderr, "%s0x%016" PRIx64 "#%" PRIu64, sample == 0 ? "" : ",", item.handle,
                     item.creation_sequence);
      }
      if (resource.count > resource.sample_count) {
        std::fputs(",...", stderr);
      }
      std::fputc(']', stderr);
    }
  }
  std::fputs("；将级联释放。\n", stderr);
}

void write_child_lifecycle_diagnostic(lifecycle_resource_type parent_type,
                                      granit_handle parent_handle,
                                      lifecycle_resource_type child_type,
                                      const lifecycle_resource_summary& children) noexcept {
  if (children.count == 0) {
    return;
  }

  std::fprintf(stderr,
               "[granit][validation] %s 0x%016" PRIx64 " 销毁时仍有 %" PRIu64 " 个用户 %s：",
               resource_names[to_index(parent_type)], parent_handle, children.count,
               resource_names[to_index(child_type)]);
  for (std::size_t sample = 0; sample < children.sample_count; ++sample) {
    const auto& item = children.samples[sample];
    std::fprintf(stderr, "%s0x%016" PRIx64 "#%" PRIu64, sample == 0 ? "" : ",", item.handle,
                 item.creation_sequence);
  }
  if (children.count > children.sample_count) {
    std::fputs(",...", stderr);
  }
  std::fputs("；将级联释放。\n", stderr);
}

} // namespace granit::detail
