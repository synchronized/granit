// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "renderer/renderer_state.h"

namespace granit::detail {

granit_result renderer_state::initialize(std::string_view application_name, bool enable_validation) {
  const auto instance_result = instance_.initialize(
    {.application_name = application_name, .enable_validation = enable_validation});
  if (instance_result != GRANIT_SUCCESS) {
    return instance_result;
  }

  const auto device_result = device_.initialize(instance_);
  if (device_result != GRANIT_SUCCESS) {
    instance_.reset();
    return device_result;
  }
  return GRANIT_SUCCESS;
}

} // namespace granit::detail
