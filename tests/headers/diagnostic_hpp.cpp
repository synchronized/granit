// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/diagnostic.hpp>

#include <type_traits>

static_assert(std::is_convertible_v<granit::diagnostic_callback, granit_diagnostic_callback>);
static_assert(static_cast<std::uint32_t>(granit::diagnostic_category::lifecycle) ==
              GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE);
