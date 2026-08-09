// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/result.h>

extern "C" const char* granit_result_message(granit_result result) {
  switch (result) {
  case GRANIT_SUCCESS:
    return "success";
  case GRANIT_ERROR_UNKNOWN:
    return "unknown error";
  case GRANIT_ERROR_INVALID_ARGUMENT:
    return "invalid argument";
  case GRANIT_ERROR_INVALID_HANDLE:
    return "invalid handle";
  case GRANIT_ERROR_OUT_OF_MEMORY:
    return "out of memory";
  case GRANIT_ERROR_UNSUPPORTED:
    return "unsupported operation";
  case GRANIT_ERROR_DEVICE_LOST:
    return "device lost";
  case GRANIT_ERROR_INTERNAL:
    return "internal error";
  case GRANIT_ERROR_BACKEND_UNAVAILABLE:
    return "rendering backend unavailable";
  case GRANIT_ERROR_INCOMPATIBLE_DRIVER:
    return "incompatible graphics driver";
  case GRANIT_ERROR_INITIALIZATION_FAILED:
    return "initialization failed";
  case GRANIT_ERROR_NO_SUITABLE_DEVICE:
    return "no suitable graphics device";
  case GRANIT_ERROR_SURFACE_LOST:
    return "window surface lost";
  case GRANIT_ERROR_OUT_OF_DATE:
    return "swapchain out of date";
  case GRANIT_ERROR_NOT_READY:
    return "operation temporarily not ready";
  default:
    return "unrecognized result";
  }
}
