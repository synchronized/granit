// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INPUT_EXPORT_H_
#define GRANIT_INPUT_EXPORT_H_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_INPUT_STATIC_DEFINE)
#define GRANIT_INPUT_API
#elif defined(GRANIT_INPUT_BUILDING_LIBRARY)
#define GRANIT_INPUT_API __declspec(dllexport)
#else
#define GRANIT_INPUT_API __declspec(dllimport)
#endif
#elif defined(GRANIT_INPUT_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define GRANIT_INPUT_API __attribute__((visibility("default")))
#else
#define GRANIT_INPUT_API
#endif

#endif
