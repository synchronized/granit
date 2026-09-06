// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/upload_batch.h>

static granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;
static granit_upload_batch_info info = GRANIT_UPLOAD_BATCH_INFO_INIT;

int granit_upload_batch_h_compiles(void) {
  granit_async_operation operation = GRANIT_NULL_HANDLE;
  (void)granit_upload_batch_submit_async(GRANIT_NULL_HANDLE, GRANIT_NULL_HANDLE, &operation);
  return desc.struct_size == GRANIT_UPLOAD_BATCH_DESC_VERSION_2_SIZE &&
         info.struct_size == GRANIT_UPLOAD_BATCH_INFO_VERSION_1_SIZE;
}
