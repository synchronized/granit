// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_EXPORT_H_
#define GRANIT_EXPORT_H_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_STATIC_DEFINE)
#define GRANIT_API
#elif defined(GRANIT_BUILDING_LIBRARY)
#define GRANIT_API __declspec(dllexport)
#else
#define GRANIT_API __declspec(dllimport)
#endif
#define GRANIT_LOCAL
#else
#if defined(GRANIT_STATIC_DEFINE)
#define GRANIT_API
#elif defined(GRANIT_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define GRANIT_API __attribute__((visibility("default")))
#else
#define GRANIT_API
#endif
#if defined(__GNUC__) || defined(__clang__)
#define GRANIT_LOCAL __attribute__((visibility("hidden")))
#else
#define GRANIT_LOCAL
#endif
#endif

#endif
