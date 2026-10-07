// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SRC_RENDERER_RESOURCE_TABLE_HPP_
#define GRANIT_SRC_RENDERER_RESOURCE_TABLE_HPP_

#include <cstdint>
#include <optional>
#include <vector>

namespace granit::internal {

enum class resource_table_result : std::uint8_t {
  success,
  invalid_handle,
  type_mismatch,
  owner_mismatch,
  capacity_exhausted,
};

enum class resource_table_type : std::uint8_t { texture_view, sampler };

struct resource_table_handle {
  std::uint64_t value{};

  [[nodiscard]] constexpr explicit operator bool() const noexcept { return value != 0; }
  [[nodiscard]] constexpr auto operator==(const resource_table_handle&) const noexcept -> bool =
      default;
};

struct resource_table_entry {
  std::uint64_t resource{};
  std::uint64_t owner{};
  resource_table_type type{};
};

/**
 * Bindless 的 CPU 侧索引原型。
 *
 * 索引只在所属 Renderer 内有效。释放后的槽位必须等到完成点到达后才能复用，避免 GPU
 * 仍在读取旧索引时观察到新资源。该类型不包含 Vulkan 或后端对象。
 */
class resource_table final {
public:
  explicit resource_table(std::uint32_t capacity) : slots_(capacity + 1) {}

  [[nodiscard]] std::optional<resource_table_handle> allocate(
      std::uint64_t owner, resource_table_type type, std::uint64_t resource) noexcept;

  [[nodiscard]] resource_table_result release(resource_table_handle handle,
                                              std::uint64_t retire_after) noexcept;

  [[nodiscard]] resource_table_result resolve(resource_table_handle handle,
                                              std::uint64_t owner, resource_table_type type,
                                              resource_table_entry& entry) const noexcept;

  void collect(std::uint64_t completed) noexcept;

  [[nodiscard]] std::uint32_t capacity() const noexcept {
    return static_cast<std::uint32_t>(slots_.size() - 1);
  }

private:
  struct slot {
    std::uint32_t generation{1};
    std::uint64_t resource{};
    std::uint64_t owner{};
    std::uint64_t retire_after{};
    resource_table_type type{};
    bool occupied{};
    bool retired{};
  };

  [[nodiscard]] static resource_table_handle make_handle(std::uint32_t index,
                                                          std::uint32_t generation) noexcept {
    return {static_cast<std::uint64_t>(generation) << 32 | index};
  }

  [[nodiscard]] static std::uint32_t index(resource_table_handle handle) noexcept {
    return static_cast<std::uint32_t>(handle.value);
  }

  [[nodiscard]] static std::uint32_t generation(resource_table_handle handle) noexcept {
    return static_cast<std::uint32_t>(handle.value >> 32);
  }

  std::vector<slot> slots_;
};

inline std::optional<resource_table_handle> resource_table::allocate(
    std::uint64_t owner, resource_table_type type, std::uint64_t resource) noexcept {
  if (owner == 0 || resource == 0) return std::nullopt;
  for (std::uint32_t index = 1; index < slots_.size(); ++index) {
    auto& value = slots_[index];
    if (value.occupied || value.retired) continue;
    value.resource = resource;
    value.owner = owner;
    value.type = type;
    value.retire_after = 0;
    value.occupied = true;
    return make_handle(index, value.generation);
  }
  return std::nullopt;
}

inline resource_table_result resource_table::release(resource_table_handle handle,
                                                      std::uint64_t retire_after) noexcept {
  const auto slot_index = index(handle);
  if (!handle || slot_index == 0 || slot_index >= slots_.size())
    return resource_table_result::invalid_handle;
  auto& value = slots_[slot_index];
  if (!value.occupied || value.generation != generation(handle))
    return resource_table_result::invalid_handle;
  value.occupied = false;
  value.retired = true;
  value.retire_after = retire_after;
  return resource_table_result::success;
}

inline resource_table_result resource_table::resolve(resource_table_handle handle,
                                                      std::uint64_t owner,
                                                      resource_table_type type,
                                                      resource_table_entry& entry) const noexcept {
  const auto slot_index = index(handle);
  if (!handle || slot_index == 0 || slot_index >= slots_.size())
    return resource_table_result::invalid_handle;
  const auto& value = slots_[slot_index];
  if (!value.occupied || value.generation != generation(handle))
    return resource_table_result::invalid_handle;
  if (value.owner != owner) return resource_table_result::owner_mismatch;
  if (value.type != type) return resource_table_result::type_mismatch;
  entry = {.resource = value.resource, .owner = value.owner, .type = value.type};
  return resource_table_result::success;
}

inline void resource_table::collect(std::uint64_t completed) noexcept {
  for (std::uint32_t index = 1; index < slots_.size(); ++index) {
    auto& value = slots_[index];
    if (!value.retired || value.retire_after > completed) continue;
    value.resource = 0;
    value.owner = 0;
    value.retire_after = 0;
    value.retired = false;
    value.generation = value.generation == UINT32_MAX ? 1 : value.generation + 1;
  }
}

} // namespace granit::internal

#endif
