// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/result.hpp>

static_assert(granit::succeeded(granit::result::success));
static_assert(granit::result::success.ok());
static_assert(!granit::result::success.failed());
static_assert(static_cast<bool>(granit::result::success));
static_assert(!static_cast<bool>(granit::result::invalid_argument));
static_assert(granit::result{}.failed());
