#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors
set -euo pipefail

# 用法: scripts/ci.sh <workflow> [ref] [suite]
# 用 gh CLI 触发对应 workflow。
#
# 可用: compatibility linux windows emscripten quick-check documentation package-shader-toolchain
#
# 示例:
#   scripts/ci.sh linux
#   scripts/ci.sh quick-check

if [ $# -lt 1 ] || [ $# -gt 3 ]; then
  echo "用法: scripts/ci.sh <workflow> [ref] [suite]" >&2
  echo "可用: compatibility linux windows emscripten quick-check documentation package-shader-toolchain" >&2
  exit 1
fi

case "$1" in
  compatibility) wf="compatibility.yml" ;;
  linux) wf="linux.yml" ;;
  windows) wf="windows.yml" ;;
  emscripten) wf="emscripten.yml" ;;
  quick-check) wf="quick-check.yml" ;;
  documentation) wf="documentation.yml" ;;
  package-shader-toolchain) wf="package-shader-toolchain.yml" ;;
  *)
    echo "错误: 未知 workflow '$1'" >&2
    exit 1
    ;;
esac

args=(workflow run "$wf")
if [[ $# -ge 2 ]]; then args+=(--ref "$2"); fi
if [[ "$1" == compatibility ]]; then args+=(-f "suite=${3:-all}"); fi
gh "${args[@]}"
echo "已触发 $wf"
