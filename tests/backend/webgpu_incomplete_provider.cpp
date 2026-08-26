// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#if defined(_WIN32)
#define GRANIT_TEST_PROVIDER_EXPORT __declspec(dllexport)
#else
#define GRANIT_TEST_PROVIDER_EXPORT __attribute__((visibility("default")))
#endif

extern "C" GRANIT_TEST_PROVIDER_EXPORT void granitTestProviderMarker() {}
