// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <fstream>
#include <string>
#include <vector>

#include <catch2/catch_all.hpp>

#include "snapshots/0.1.0/core_identity.h"
#include "snapshots/0.1.0/optional_components_identity.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

std::vector<std::string> load_symbols(const char* path) {
  std::ifstream input{path};
  REQUIRE(input.is_open());

  std::vector<std::string> symbols;
  for (std::string symbol; std::getline(input, symbol);) {
    if (!symbol.empty()) {
      symbols.push_back(std::move(symbol));
    }
  }
  REQUIRE(!symbols.empty());
  return symbols;
}

class shared_library {
public:
  explicit shared_library(const char* path) {
#if defined(_WIN32)
    handle_ = LoadLibraryA(path);
#else
    handle_ = dlopen(path, RTLD_NOW | RTLD_LOCAL);
#endif
  }

  ~shared_library() {
    if (handle_ == nullptr) {
      return;
    }
#if defined(_WIN32)
    FreeLibrary(handle_);
#else
    dlclose(handle_);
#endif
  }

  shared_library(const shared_library&) = delete;
  shared_library& operator=(const shared_library&) = delete;

  [[nodiscard]] bool is_open() const { return handle_ != nullptr; }

  [[nodiscard]] bool contains(const char* symbol) const {
#if defined(_WIN32)
    return GetProcAddress(handle_, symbol) != nullptr;
#else
    return dlsym(handle_, symbol) != nullptr;
#endif
  }

private:
#if defined(_WIN32)
  HMODULE handle_{nullptr};
#else
  void* handle_{nullptr};
#endif
};

} // namespace

void check_exports(const char* component, const char* path, const char* snapshot_path) {
  INFO("component: " << component);
  const shared_library library{path};
  REQUIRE(library.is_open());

  const auto symbols = load_symbols(snapshot_path);
  for (const auto& symbol : symbols) {
    INFO("缺少公共导出符号: " << symbol);
    CHECK(library.contains(symbol.c_str()));
  }
}

TEST_CASE("共享库导出完整的公共 C ABI", "[abi][exports]") {
  INFO("ABI 快照: " << GRANIT_ABI_SNAPSHOT_COMPONENT << " " << GRANIT_ABI_SNAPSHOT_VERSION);
  check_exports("Core", GRANIT_ABI_CORE_LIBRARY_PATH, GRANIT_ABI_CORE_SYMBOLS_PATH);
  check_exports(GRANIT_ABI_SNAPSHOT_RENDER_PIPELINE_COMPONENT,
                GRANIT_ABI_RENDER_PIPELINE_LIBRARY_PATH, GRANIT_ABI_RENDER_PIPELINE_SYMBOLS_PATH);
  check_exports(GRANIT_ABI_SNAPSHOT_WINDOW_COMPONENT, GRANIT_ABI_WINDOW_LIBRARY_PATH,
                GRANIT_ABI_WINDOW_SYMBOLS_PATH);
  check_exports(GRANIT_ABI_SNAPSHOT_INPUT_COMPONENT, GRANIT_ABI_INPUT_LIBRARY_PATH,
                GRANIT_ABI_INPUT_SYMBOLS_PATH);
}
