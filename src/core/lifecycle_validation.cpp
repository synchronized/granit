// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "core/lifecycle_validation.h"

#include "core/diagnostic_sink.h"

#include <algorithm>
#include <array>
#include <cinttypes>
#include <cstdio>

namespace granit::detail {
namespace {

constexpr std::array<const char*, static_cast<std::size_t>(lifecycle_resource_type::count)>
    resource_names{"Buffer",           "Texture",         "TextureView",       "Sampler",
                   "Shader",           "PipelineLayout",  "BindGroupLayout",   "BindGroup",
                   "GraphicsPipeline", "ComputePipeline", "Surface",           "Swapchain",
                   "CommandRecorder",  "UploadBatch",     "TimestampQueryPool"};

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

void write_lifecycle_diagnostic(const diagnostic_sink& diagnostics, granit_renderer renderer,
                                std::uint32_t domain, const lifecycle_snapshot& snapshot) noexcept {
  if (snapshot.empty()) {
    return;
  }

  std::array<char, 8192> message{};
  std::size_t used{};
  const auto append = [&]<typename... Arguments>(const char* format, Arguments... arguments) {
    if (used >= message.size() - 1) {
      return;
    }
    const auto written = [&] {
      if constexpr (sizeof...(Arguments) == 0)
        return std::snprintf(message.data() + used, message.size() - used, "%s", format);
      else
        return std::snprintf(message.data() + used, message.size() - used, format, arguments...);
    }();
    if (written > 0) {
      used += std::min(static_cast<std::size_t>(written), message.size() - used - 1);
    }
  };
  append("Renderer 0x%016" PRIx64 " (domain=%" PRIu32 ") 销毁时仍有 %" PRIu64 " 个用户资源：",
         renderer, domain, snapshot.total_count());
  bool first = true;
  for (std::size_t index = 0; index < resource_names.size(); ++index) {
    const auto type = static_cast<lifecycle_resource_type>(index);
    const auto& resource = snapshot.summary(type);
    if (resource.count == 0) {
      continue;
    }
    append("%s%s=%" PRIu64, first ? "" : ", ", resource_names[index], resource.count);
    first = false;
    if (resource.sample_count != 0) {
      append(" [");
      for (std::size_t sample = 0; sample < resource.sample_count; ++sample) {
        const auto& item = resource.samples[sample];
        append("%s0x%016" PRIx64 "#%" PRIu64, sample == 0 ? "" : ",", item.handle,
               item.creation_sequence);
      }
      if (resource.count > resource.sample_count) {
        append(",...");
      }
      append("]");
    }
  }
  append("；将级联释放。");
  diagnostics.emit(diagnostic_severity::warning, diagnostic_category::lifecycle,
                   {message.data(), used});
}

void write_child_lifecycle_diagnostic(const diagnostic_sink& diagnostics,
                                      lifecycle_resource_type parent_type,
                                      granit_handle parent_handle,
                                      lifecycle_resource_type child_type,
                                      const lifecycle_resource_summary& children) noexcept {
  if (children.count == 0) {
    return;
  }

  std::array<char, 1024> message{};
  std::size_t used{};
  const auto append = [&]<typename... Arguments>(const char* format, Arguments... arguments) {
    if (used >= message.size() - 1) {
      return;
    }
    const auto written = [&] {
      if constexpr (sizeof...(Arguments) == 0)
        return std::snprintf(message.data() + used, message.size() - used, "%s", format);
      else
        return std::snprintf(message.data() + used, message.size() - used, format, arguments...);
    }();
    if (written > 0) {
      used += std::min(static_cast<std::size_t>(written), message.size() - used - 1);
    }
  };
  append("%s 0x%016" PRIx64 " 销毁时仍有 %" PRIu64 " 个用户 %s：",
         resource_names[to_index(parent_type)], parent_handle, children.count,
         resource_names[to_index(child_type)]);
  for (std::size_t sample = 0; sample < children.sample_count; ++sample) {
    const auto& item = children.samples[sample];
    append("%s0x%016" PRIx64 "#%" PRIu64, sample == 0 ? "" : ",", item.handle,
           item.creation_sequence);
  }
  if (children.count > children.sample_count) {
    append(",...");
  }
  append("；将级联释放。");
  diagnostics.emit(diagnostic_severity::warning, diagnostic_category::lifecycle,
                   {message.data(), used});
}

} // namespace granit::detail
