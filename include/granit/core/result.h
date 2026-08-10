// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_RESULT_H_
#define GRANIT_RESULT_H_

#include <stdint.h>

#include <granit/core/export.h>

/** Granit 操作结果的定宽 ABI 类型。零表示成功，负值表示失败。 */
typedef int32_t granit_result;

#define GRANIT_SUCCESS INT32_C(0)
#define GRANIT_ERROR_UNKNOWN INT32_C(-1)
#define GRANIT_ERROR_INVALID_ARGUMENT INT32_C(-2)
#define GRANIT_ERROR_INVALID_HANDLE INT32_C(-3)
#define GRANIT_ERROR_OUT_OF_MEMORY INT32_C(-4)
#define GRANIT_ERROR_UNSUPPORTED INT32_C(-5)
#define GRANIT_ERROR_DEVICE_LOST INT32_C(-6)
#define GRANIT_ERROR_INTERNAL INT32_C(-7)
#define GRANIT_ERROR_BACKEND_UNAVAILABLE INT32_C(-8)
#define GRANIT_ERROR_INCOMPATIBLE_DRIVER INT32_C(-9)
#define GRANIT_ERROR_INITIALIZATION_FAILED INT32_C(-10)
#define GRANIT_ERROR_NO_SUITABLE_DEVICE INT32_C(-11)
#define GRANIT_ERROR_SURFACE_LOST INT32_C(-12)
#define GRANIT_ERROR_OUT_OF_DATE INT32_C(-13)
#define GRANIT_ERROR_NOT_READY INT32_C(-14)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 返回结果码对应的静态英文文本。
 *
 * 返回的字符串由 Granit 持有，调用者不得释放。该函数线程安全且不返回空指针。
 */
GRANIT_API const char* granit_result_message(granit_result result);

#ifdef __cplusplus
}
#endif

#endif
