// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Granit contributors

#include <granit/renderer/resource_table.h>

int granit_resource_table_c_header_test(void) {
  granit_resource_table_desc desc = GRANIT_RESOURCE_TABLE_DESC_INIT;
  granit_resource_table_capabilities capabilities = GRANIT_RESOURCE_TABLE_CAPABILITIES_INIT;
  granit_resource_table_update_desc update = {0};
  return (int)(desc.struct_size + capabilities.struct_size + update.slot);
}
