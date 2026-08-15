// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_DIAGNOSTIC_H_
#define GRANIT_DIAGNOSTIC_H_

#include <stdint.h>

/** 诊断消息严重级别；数值不对应 Vulkan 枚举。 */
typedef uint32_t granit_diagnostic_severity;
#define GRANIT_DIAGNOSTIC_SEVERITY_INFO UINT32_C(1)
#define GRANIT_DIAGNOSTIC_SEVERITY_WARNING UINT32_C(2)
#define GRANIT_DIAGNOSTIC_SEVERITY_ERROR UINT32_C(3)

/** 诊断消息类别；数值不对应 Vulkan 标志。 */
typedef uint32_t granit_diagnostic_category;
#define GRANIT_DIAGNOSTIC_CATEGORY_GENERAL UINT32_C(1)
#define GRANIT_DIAGNOSTIC_CATEGORY_VALIDATION UINT32_C(2)
#define GRANIT_DIAGNOSTIC_CATEGORY_PERFORMANCE UINT32_C(3)
#define GRANIT_DIAGNOSTIC_CATEGORY_LIFECYCLE UINT32_C(4)
#define GRANIT_DIAGNOSTIC_CATEGORY_DEVICE UINT32_C(5)

/**
 * 同步诊断回调。message 只在调用期间有效且不保证以零结尾。
 *
 * 回调可能由多个线程并发调用，不得重入产生消息的同一 Renderer。user_data 由调用者持有，
 * 有效期必须覆盖 Renderer 创建、使用和销毁全过程。
 */
typedef void (*granit_diagnostic_callback)(granit_diagnostic_severity severity,
                                           granit_diagnostic_category category, const char* message,
                                           uint32_t message_length, void* user_data);

#endif
