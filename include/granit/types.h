// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TYPES_H_
#define GRANIT_TYPES_H_

#include <stdint.h>

/** Granit 资源句柄的基础存储类型。句柄只在当前进程和库生命周期内有效。 */
typedef uint64_t granit_handle;

/** 统一的无效句柄值。 */
#define GRANIT_NULL_HANDLE UINT64_C(0)

#endif
