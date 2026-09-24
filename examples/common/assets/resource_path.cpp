// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/resource_path.h"

#include <utility>

namespace granit::example::assets {

bool normalize_resource_path(std::string_view source, std::string& normalized) {
  if (source.empty() || source.front() == '/' || source.front() == '\\' ||
      source.find_first_of(":?#%\\") != std::string_view::npos) {
    return false;
  }

  std::string candidate;
  candidate.reserve(source.size());
  std::size_t offset = 0;
  while (offset < source.size()) {
    const auto separator = source.find('/', offset);
    const auto component = source.substr(
        offset, separator == std::string_view::npos ? source.size() - offset : separator - offset);
    if (component == "..")
      return false;
    if (!component.empty() && component != ".") {
      if (!candidate.empty())
        candidate.push_back('/');
      candidate.append(component);
    }
    if (separator == std::string_view::npos)
      break;
    offset = separator + 1;
  }
  if (candidate.empty())
    return false;
  normalized = std::move(candidate);
  return true;
}

} // namespace granit::example::assets
