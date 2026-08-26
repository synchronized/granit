// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_TEST_WEBGPU_H_
#define GRANIT_TEST_WEBGPU_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WGPUInstanceImpl* WGPUInstance;

WGPUInstance wgpuCreateInstance(const void* descriptor);
void wgpuInstanceRelease(WGPUInstance instance);

#ifdef __cplusplus
}
#endif

#endif
