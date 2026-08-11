// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/upload_batch.h>

static granit_upload_batch_desc desc = GRANIT_UPLOAD_BATCH_DESC_INIT;

int granit_upload_batch_h_compiles(void) {
  return desc.struct_size == GRANIT_UPLOAD_BATCH_DESC_VERSION_1_SIZE;
}
