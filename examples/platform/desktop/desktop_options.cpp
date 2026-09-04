// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "desktop_options.h"

#include <new>
#include <utility>

namespace granit::example::model_viewer::desktop {
namespace {

bool parse_backend(std::string_view value, granit::renderer_backend& backend) noexcept {
  if (value == "auto")
    backend = granit::renderer_backend::automatic;
  else if (value == "vulkan")
    backend = granit::renderer_backend::vulkan;
  else
    return false;
  return true;
}

bool parse_present_mode(std::string_view value, granit::present_mode& presentation) noexcept {
  if (value == "fifo")
    presentation = granit::present_mode::fifo;
  else if (value == "mailbox")
    presentation = granit::present_mode::mailbox;
  else if (value == "immediate")
    presentation = granit::present_mode::immediate;
  else
    return false;
  return true;
}

} // namespace

granit::result parse_options(std::span<const std::string_view> arguments, options& output) {
  try {
    options candidate;
    for (std::size_t index = 0; index < arguments.size(); ++index) {
      const auto argument = arguments[index];
      constexpr std::string_view backend_prefix = "--backend=";
      if (argument.starts_with(backend_prefix)) {
        if (!parse_backend(argument.substr(backend_prefix.size()), candidate.backend))
          return granit::result::invalid_argument;
      } else if (argument == "--asset" && index + 1 < arguments.size()) {
        candidate.asset_path = arguments[++index];
        if (candidate.asset_path.empty())
          return granit::result::invalid_argument;
      } else if (argument == "--environment" && index + 1 < arguments.size()) {
        candidate.environment_path = arguments[++index];
        if (candidate.environment_path.empty())
          return granit::result::invalid_argument;
      } else if (argument == "--validation") {
        candidate.enable_validation = true;
      } else if (argument == "--smoke-test") {
        candidate.smoke_test = true;
      } else if (argument == "--no-ui") {
        candidate.show_ui = false;
      } else if (argument == "--profile-output" && index + 1 < arguments.size()) {
        candidate.profile_output_path = arguments[++index];
        if (candidate.profile_output_path.empty())
          return granit::result::invalid_argument;
      } else {
        constexpr std::string_view presentation_prefix = "--present-mode=";
        if (!argument.starts_with(presentation_prefix) ||
            !parse_present_mode(argument.substr(presentation_prefix.size()),
                                candidate.presentation)) {
          return granit::result::invalid_argument;
        }
      }
    }
    if (candidate.asset_path.empty())
      return granit::result::invalid_argument;
    if (candidate.smoke_test && !candidate.profile_output_path.empty())
      return granit::result::invalid_argument;
    output = std::move(candidate);
    return granit::result::success;
  } catch (const std::bad_alloc&) {
    return granit::result::out_of_memory;
  }
}

} // namespace granit::example::model_viewer::desktop
