#!/usr/bin/env bash
# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors
set -euo pipefail
# 用法：scripts/dev.sh <preset> [--test] [--suite default|quick|stress] [--seconds N] [--seed N]
# 可选 --iterations N 限制单项迭代数；0 表示仅按时长停止。
[[ $# -ge 1 ]] || { echo '缺少 preset' >&2; exit 2; }
preset="$1"; shift
run_test=false
suite=default
seconds=3600
seed=20261024
iterations=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --test) run_test=true; shift ;;
    --suite) suite="${2:?缺少范围}"; run_test=true; shift 2 ;;
    --seconds) seconds="${2:?缺少秒数}"; shift 2 ;;
    --seed) seed="${2:?缺少种子}"; shift 2 ;;
    --iterations) iterations="${2:?缺少迭代上限}"; shift 2 ;;
    *) echo "未知参数：$1" >&2; exit 2 ;;
  esac
done
repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"
if [[ "$run_test" == true ]]; then
  exec cmake "-DGRANIT_SOURCE_DIR=$repo_root" "-DGRANIT_PRESET=$preset" \
    "-DGRANIT_SUITE=$suite" "-DGRANIT_STRESS_SECONDS=$seconds" \
    "-DGRANIT_STRESS_SEED=$seed" "-DGRANIT_STRESS_ITERATIONS=$iterations" -P scripts/validate.cmake
fi
cmake --preset "$preset"
cmake --build --preset "$preset"
