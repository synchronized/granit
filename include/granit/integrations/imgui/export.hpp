// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_INTEGRATIONS_IMGUI_EXPORT_HPP_
#define GRANIT_INTEGRATIONS_IMGUI_EXPORT_HPP_

#if defined(_WIN32) || defined(__CYGWIN__)
#if defined(GRANIT_INTEGRATION_IMGUI_STATIC_DEFINE)
#define GRANIT_INTEGRATION_IMGUI_API
#elif defined(GRANIT_INTEGRATION_IMGUI_BUILDING_LIBRARY)
#define GRANIT_INTEGRATION_IMGUI_API __declspec(dllexport)
#else
#define GRANIT_INTEGRATION_IMGUI_API __declspec(dllimport)
#endif
#elif defined(__GNUC__) && __GNUC__ >= 4
#define GRANIT_INTEGRATION_IMGUI_API __attribute__((visibility("default")))
#else
#define GRANIT_INTEGRATION_IMGUI_API
#endif

#endif
