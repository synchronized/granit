// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#ifndef GRANIT_WINDOW_LOOP_H_
#define GRANIT_WINDOW_LOOP_H_

#include <stddef.h>
#include <stdint.h>

#include <granit/core/result.h>
#include <granit/window/export.h>
#include <granit/window/window.h>

#if defined(_MSC_VER)
#define GRANIT_WINDOW_CALLBACK __cdecl
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
#define GRANIT_WINDOW_CALLBACK __attribute__((cdecl))
#else
#define GRANIT_WINDOW_CALLBACK
#endif

typedef uint32_t granit_window_loop_action;
#define GRANIT_WINDOW_LOOP_CONTINUE UINT32_C(0)
#define GRANIT_WINDOW_LOOP_IDLE UINT32_C(1)
#define GRANIT_WINDOW_LOOP_STOP UINT32_C(2)

/** 每轮事件处理后调用；回调只在 Window System 创建线程执行。 */
typedef granit_result(GRANIT_WINDOW_CALLBACK* granit_window_loop_tick_fn)(
    void* user_data, granit_window_loop_action* action);

/** Loop 停止时恰好调用一次；user_data 由宿主持有并负责释放。 */
typedef void(GRANIT_WINDOW_CALLBACK* granit_window_loop_shutdown_fn)(void* user_data,
                                                                    granit_result reason);

typedef struct granit_window_loop_desc {
  uint32_t struct_size;
  granit_window_loop_tick_fn tick;
  granit_window_loop_shutdown_fn shutdown;
  void* user_data;
  uint32_t flags;
  uint32_t reserved;
} granit_window_loop_desc;

#define GRANIT_WINDOW_LOOP_DESC_VERSION_1_SIZE                                                    \
  ((uint32_t)(offsetof(granit_window_loop_desc, reserved) + sizeof(uint32_t)))
#define GRANIT_WINDOW_LOOP_DESC_INIT                                                              \
  {(uint32_t)sizeof(granit_window_loop_desc), 0, 0, 0, UINT32_C(0), UINT32_C(0)}

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 在当前线程运行可选托管 Loop。
 *
 * 桌面平台阻塞到 Tick 请求停止或失败。Emscripten 将控制权交给浏览器主循环。接受描述后，停止时
 * 必定调用一次 shutdown；调用期间 user_data 必须有效。Renderer 和应用资源仍由宿主 Tick 管理。
 */
GRANIT_WINDOW_API granit_result
granit_window_system_run_loop(granit_window_system window_system,
                              const granit_window_loop_desc* desc);

#ifdef __cplusplus
}
#endif

#endif
