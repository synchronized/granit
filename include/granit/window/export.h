// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_EXPORT_H_
#define GRANIT_WINDOW_EXPORT_H_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_WINDOW_STATIC_DEFINE)
#define GRANIT_WINDOW_API
#elif defined(GRANIT_WINDOW_BUILDING_LIBRARY)
#define GRANIT_WINDOW_API __declspec(dllexport)
#else
#define GRANIT_WINDOW_API __declspec(dllimport)
#endif
#elif defined(GRANIT_WINDOW_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define GRANIT_WINDOW_API __attribute__((visibility("default")))
#else
#define GRANIT_WINDOW_API
#endif

#endif
