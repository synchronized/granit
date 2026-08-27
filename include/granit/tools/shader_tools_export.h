// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_SHADER_TOOLS_EXPORT_H_
#define GRANIT_SHADER_TOOLS_EXPORT_H_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_SHADER_TOOLS_STATIC_DEFINE)
#define GRANIT_SHADER_TOOLS_API
#elif defined(GRANIT_SHADER_TOOLS_BUILDING_LIBRARY)
#define GRANIT_SHADER_TOOLS_API __declspec(dllexport)
#else
#define GRANIT_SHADER_TOOLS_API __declspec(dllimport)
#endif
#elif defined(GRANIT_SHADER_TOOLS_BUILDING_LIBRARY) && (defined(__GNUC__) || defined(__clang__))
#define GRANIT_SHADER_TOOLS_API __attribute__((visibility("default")))
#else
#define GRANIT_SHADER_TOOLS_API
#endif

#endif
