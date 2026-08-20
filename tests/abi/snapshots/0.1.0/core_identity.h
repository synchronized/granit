// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TESTS_ABI_SNAPSHOTS_0_1_0_CORE_IDENTITY_H_
#define GRANIT_TESTS_ABI_SNAPSHOTS_0_1_0_CORE_IDENTITY_H_

#include <granit/core/version.h>

#if GRANIT_VERSION_MAJOR != 0 || GRANIT_VERSION_MINOR != 1 || GRANIT_VERSION_PATCH != 0
#error "当前核心 ABI 基线只适用于 Granit 0.1.0"
#endif

#if !defined(_WIN32) && !defined(__linux__)
#error "当前核心 ABI 基线只覆盖 Windows 和 Linux"
#endif

#if !defined(_M_X64) && !defined(__x86_64__)
#error "当前核心 ABI 基线只覆盖 x86_64"
#endif

#if !defined(_MSC_VER) && !defined(__clang__) && !defined(__GNUC__)
#error "当前核心 ABI 基线只覆盖 MSVC、Clang 和 GCC"
#endif

#define GRANIT_ABI_SNAPSHOT_VERSION "0.1.0"
#define GRANIT_ABI_SNAPSHOT_COMPONENT "Core"

#endif
