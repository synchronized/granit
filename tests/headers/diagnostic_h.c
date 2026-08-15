// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/core/diagnostic.h>

static void granit_diagnostic_header_callback(granit_diagnostic_severity severity,
                                              granit_diagnostic_category category,
                                              const char* message, uint32_t message_length,
                                              void* user_data) {
  (void)severity;
  (void)category;
  (void)message;
  (void)message_length;
  (void)user_data;
}

granit_diagnostic_callback granit_diagnostic_header_check(void) {
  return granit_diagnostic_header_callback;
}
