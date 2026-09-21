#!/usr/bin/env bash
set -euo pipefail

# 用法: scripts/dev.sh <preset> [--test]
# 等价于: cmake --preset <preset> && cmake --build --preset <preset> [&& ctest --preset <preset>]
#
# 示例:
#   scripts/dev.sh linux-clang-release
#   scripts/dev.sh linux-clang-release --test

if [ $# -lt 1 ]; then
  echo "用法: scripts/dev.sh <preset> [--test]" >&2
  echo "示例: scripts/dev.sh linux-clang-release --test" >&2
  exit 1
fi
preset="$1"
run_test=false
if [ $# -ge 2 ] && [ "$2" = "--test" ]; then
  run_test=true
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

cmake --preset "$preset"
cmake --build --preset "$preset"
if [ "$run_test" = true ]; then
  ctest --preset "$preset" --output-on-failure
fi
