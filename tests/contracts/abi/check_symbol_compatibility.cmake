# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

foreach(required_var IN ITEMS GRANIT_ABI_COMPONENT GRANIT_ABI_BASELINE GRANIT_ABI_CURRENT
                              GRANIT_ABI_POLICY)
  if(NOT DEFINED ${required_var} OR "${${required_var}}" STREQUAL "")
    message(FATAL_ERROR "缺少 ABI 兼容检查参数：${required_var}")
  endif()
endforeach()

if(NOT GRANIT_ABI_POLICY STREQUAL "compatible" AND
   NOT GRANIT_ABI_POLICY STREQUAL "experimental")
  message(FATAL_ERROR "未知 ABI 兼容策略：${GRANIT_ABI_POLICY}")
endif()

file(STRINGS "${GRANIT_ABI_BASELINE}" baseline_symbols REGEX "^granit_[A-Za-z0-9_]+$")
file(STRINGS "${GRANIT_ABI_CURRENT}" current_symbols REGEX "^granit_[A-Za-z0-9_]+$")
list(REMOVE_DUPLICATES baseline_symbols)
list(REMOVE_DUPLICATES current_symbols)
list(SORT baseline_symbols)
list(SORT current_symbols)

set(removed_symbols "${baseline_symbols}")
foreach(symbol IN LISTS current_symbols)
  list(REMOVE_ITEM removed_symbols "${symbol}")
endforeach()

set(added_symbols "${current_symbols}")
foreach(symbol IN LISTS baseline_symbols)
  list(REMOVE_ITEM added_symbols "${symbol}")
endforeach()

if(GRANIT_ABI_POLICY STREQUAL "compatible")
  if(removed_symbols)
    message(FATAL_ERROR
      "${GRANIT_ABI_COMPONENT} 是稳定候选 component，不能删除历史符号：${removed_symbols}"
    )
  endif()
elseif(DEFINED GRANIT_ABI_ALLOWED_REMOVALS AND
       EXISTS "${GRANIT_ABI_ALLOWED_REMOVALS}")
  file(STRINGS "${GRANIT_ABI_ALLOWED_REMOVALS}" allowed_removals
       REGEX "^granit_[A-Za-z0-9_]+$")
  list(REMOVE_DUPLICATES allowed_removals)
  list(SORT allowed_removals)
  if(NOT "${removed_symbols}" STREQUAL "${allowed_removals}")
    message(FATAL_ERROR
      "${GRANIT_ABI_COMPONENT} 的实验性破坏清单与实际删除不一致。"
      "\n实际删除：${removed_symbols}"
      "\n已登记：${allowed_removals}"
    )
  endif()
elseif(removed_symbols)
  message(FATAL_ERROR
    "${GRANIT_ABI_COMPONENT} 删除了实验性符号但未提供显式破坏清单：${removed_symbols}"
  )
endif()

list(LENGTH added_symbols added_count)
list(LENGTH removed_symbols removed_count)
message(STATUS
  "${GRANIT_ABI_COMPONENT} ABI 兼容检查通过：新增 ${added_count}，删除 ${removed_count}"
)
