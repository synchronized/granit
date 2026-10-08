// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <cstdint>
#include <cstdlib>
#include <fstream>

namespace {

using renderdoc_function = void (*)();

struct renderdoc_api {
  renderdoc_function prefix[15]{};
  renderdoc_function trigger_capture{};
  renderdoc_function suffix[13]{};
};

void write_marker() noexcept {
  const char* path = nullptr;
#if defined(_WIN32)
  char* allocated_path = nullptr;
  std::size_t length = 0;
  if (_dupenv_s(&allocated_path, &length, "GRANIT_RENDERDOC_SUBSTITUTE_MARKER") != 0)
    return;
  path = allocated_path;
#else
  path = std::getenv("GRANIT_RENDERDOC_SUBSTITUTE_MARKER");
#endif
  if (path == nullptr || *path == '\0') {
#if defined(_WIN32)
    std::free(allocated_path);
#endif
    return;
  }
  std::ofstream marker{path, std::ios::out | std::ios::trunc};
  if (marker)
    marker << "triggered\n";
#if defined(_WIN32)
  std::free(allocated_path);
#endif
}

renderdoc_api api{.trigger_capture = write_marker};

} // namespace

#if defined(_WIN32)
#define GRANIT_RENDERDOC_TEST_EXPORT __declspec(dllexport)
#else
#define GRANIT_RENDERDOC_TEST_EXPORT __attribute__((visibility("default")))
#endif

extern "C" GRANIT_RENDERDOC_TEST_EXPORT int RENDERDOC_GetAPI(std::uint32_t version,
                                                             void** out_api) noexcept {
  if (out_api == nullptr || version != 10000)
    return 0;
  *out_api = &api;
  return 1;
}

#undef GRANIT_RENDERDOC_TEST_EXPORT
