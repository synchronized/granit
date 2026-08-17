// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INTEGRATIONS_SDL3_EXPORT_HPP_
#define GRANIT_INTEGRATIONS_SDL3_EXPORT_HPP_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_INTEGRATION_SDL3_STATIC_DEFINE)
#define GRANIT_INTEGRATION_SDL3_API
#elif defined(GRANIT_INTEGRATION_SDL3_BUILDING_LIBRARY)
#define GRANIT_INTEGRATION_SDL3_API __declspec(dllexport)
#else
#define GRANIT_INTEGRATION_SDL3_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define GRANIT_INTEGRATION_SDL3_API __attribute__((visibility("default")))
#else
#define GRANIT_INTEGRATION_SDL3_API
#endif

#endif
