// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include "assets/asset_manager.h"
#include "platform/register_asset_sources.h"

#include <catch2/catch_all.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <unordered_map>

namespace assets = granit::example::assets;
namespace tasks = granit::example::tasks;

namespace {

class memory_source final : public assets::asset_manager_source {
public:
  void insert(std::string path, std::string value) {
    std::vector<std::byte> bytes(value.size());
    std::memcpy(bytes.data(), value.data(), value.size());
    values_.emplace(std::move(path), std::move(bytes));
  }

  [[nodiscard]] granit::result load(const assets::asset_location& location,
                                    assets::asset_source_completion completion) noexcept override {
    ++loads_;
    const auto found = values_.find(std::string{location.path()});
    if (found == values_.end()) {
      completion({.error = assets::asset_error::io_error,
                  .bytes = {},
                  .total_bytes = std::nullopt,
                  .diagnostic = "memory asset missing"});
    } else {
      completion({.error = assets::asset_error::none,
                  .bytes = found->second,
                  .total_bytes = found->second.size(),
                  .diagnostic = {}});
    }
    return granit::result::success;
  }

  [[nodiscard]] std::uint32_t loads() const noexcept { return loads_; }

private:
  std::unordered_map<std::string, std::vector<std::byte>> values_;
  std::uint32_t loads_{};
};

void finish(tasks::task_system& task_system) {
  REQUIRE(task_system.wait_idle().ok());
  while (task_system.pump_main() != 0) {
    REQUIRE(task_system.wait_idle().ok());
  }
}

} // namespace

TEST_CASE("asset manager delays inline source completion") {
  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  assets::asset_manager manager{task_system};
  auto source = std::make_shared<memory_source>();
  source->insert("hello.bin", "hello");
  REQUIRE(manager.register_source(assets::asset_scheme::memory, source).ok());
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()).ok());

  auto handle = manager.load<assets::asset_blob>(assets::asset_location::memory("hello.bin"));
  REQUIRE(handle.valid());
  REQUIRE(handle.status() != assets::asset_status::ready);
  finish(task_system);
  REQUIRE(handle.ready());
  REQUIRE(handle.value()->bytes.size() == 5);
}

TEST_CASE("asset manager merges matching requests and caches the result") {
  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  assets::asset_manager manager{task_system};
  auto source = std::make_shared<memory_source>();
  source->insert("shared.bin", "shared");
  REQUIRE(manager.register_source(assets::asset_scheme::memory, source).ok());
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()).ok());

  auto first = manager.load<assets::asset_blob>(assets::asset_location::memory("shared.bin"));
  auto second = manager.load<assets::asset_blob>(assets::asset_location::memory("shared.bin"));
  finish(task_system);
  REQUIRE(first.ready());
  REQUIRE(second.ready());
  REQUIRE(first.value() == second.value());
  REQUIRE(source->loads() == 1);

  const auto cached_value = first.value();
  first = {};
  second = {};
  auto cached = manager.load<assets::asset_blob>(assets::asset_location::memory("shared.bin"));
  REQUIRE(cached.ready());
  REQUIRE(cached.value() == cached_value);
  REQUIRE(source->loads() == 1);
}

TEST_CASE("asset manager isolates observer cancellation") {
  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  assets::asset_manager manager{task_system};
  auto source = std::make_shared<memory_source>();
  source->insert("shared.bin", "shared");
  REQUIRE(manager.register_source(assets::asset_scheme::memory, source).ok());
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()).ok());

  auto cancelled = manager.load<assets::asset_blob>(assets::asset_location::memory("shared.bin"));
  auto active = manager.load<assets::asset_blob>(assets::asset_location::memory("shared.bin"));
  cancelled.cancel();
  finish(task_system);
  REQUIRE(cancelled.status() == assets::asset_status::cancelled);
  REQUIRE(active.ready());
}

TEST_CASE("asset manager locks registration after the first load") {
  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  assets::asset_manager manager{task_system};
  auto source = std::make_shared<memory_source>();
  REQUIRE(manager.register_source(assets::asset_scheme::memory, source).ok());
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()).ok());
  static_cast<void>(manager.load<assets::asset_blob>(assets::asset_location::memory("missing")));
  REQUIRE(manager.register_source(assets::asset_scheme::bundled, source) ==
          granit::result::invalid_argument);
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()) ==
          granit::result::invalid_argument);
}

TEST_CASE("asset manager reports missing registrations") {
  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 0}).ok());
  assets::asset_manager manager{task_system};
  auto handle = manager.load<assets::asset_blob>(assets::asset_location::memory("missing"));
  REQUIRE(handle.status() == assets::asset_status::failed);
  REQUIRE(handle.error() == assets::asset_error::source_not_registered);
}

#if !defined(__EMSCRIPTEN__)
TEST_CASE("desktop platform source reads an external file") {
  const auto path = std::filesystem::temp_directory_path() / "granit_asset_manager_source.bin";
  {
    std::ofstream stream{path, std::ios::binary};
    REQUIRE(stream.good());
    stream << "platform";
  }

  tasks::task_system task_system;
  REQUIRE(task_system.initialize({.worker_count = 1}).ok());
  assets::asset_manager manager{task_system};
  REQUIRE(granit::example::platform::register_asset_sources(manager, "test.exe").ok());
  REQUIRE(manager.register_loader(std::make_shared<assets::blob_asset_loader>()).ok());
  auto handle = manager.load<assets::asset_blob>(
      assets::asset_location::external(path.string()));
  finish(task_system);
  REQUIRE(handle.ready());
  REQUIRE(handle.value()->bytes.size() == 8);

  std::error_code error;
  std::filesystem::remove(path, error);
}
#endif
