// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <webgpu/webgpu.h>

struct WGPUInstanceImpl {};

extern "C" WGPUInstance wgpuCreateInstance(const void*) { return new WGPUInstanceImpl; }

extern "C" void wgpuInstanceRelease(WGPUInstance instance) { delete instance; }
