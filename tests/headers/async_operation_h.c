// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/async_operation.h>

typedef char granit_async_operation_status_size_check
    [sizeof(granit_async_operation_status) == GRANIT_ASYNC_OPERATION_STATUS_VERSION_1_SIZE ? 1
                                                                                           : -1];
