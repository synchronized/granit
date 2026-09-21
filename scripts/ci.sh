#!/usr/bin/env bash
set -euo pipefail

# 用法: scripts/ci.sh <workflow>
# 用 gh CLI 触发对应 workflow。
#
# 可用: linux windows emscripten quick-check documentation release package-shader-toolchain
#
# 示例:
#   scripts/ci.sh linux
#   scripts/ci.sh quick-check

if [ $# -ne 1 ]; then
  echo "用法: scripts/ci.sh <workflow>" >&2
  echo "可用: linux windows emscripten quick-check documentation release package-shader-toolchain" >&2
  exit 1
fi

case "$1" in
  linux) wf="linux.yml" ;;
  windows) wf="windows.yml" ;;
  emscripten) wf="emscripten.yml" ;;
  quick-check) wf="quick-check.yml" ;;
  documentation) wf="documentation.yml" ;;
  release) wf="release.yml" ;;
  package-shader-toolchain) wf="package-shader-toolchain.yml" ;;
  *)
    echo "错误: 未知 workflow '$1'" >&2
    exit 1
    ;;
esac

gh workflow run "$wf"
echo "已触发 $wf"
