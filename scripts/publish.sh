#!/usr/bin/env bash
set -euo pipefail

# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "用法: scripts/publish.sh <version> [--no-wait]" >&2
  exit 1
fi
version="$1"
wait_for_run=1
if [ $# -eq 2 ]; then
  if [ "$2" != "--no-wait" ]; then
    echo "错误: 未知参数 $2" >&2
    exit 1
  fi
  wait_for_run=0
fi

if ! echo "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "错误: 版本号格式应为 x.y.z，例如 0.27.0" >&2
  exit 1
fi
if ! command -v gh >/dev/null 2>&1; then
  echo "错误: 未找到 gh CLI" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [ "$(git branch --show-current)" != "main" ]; then
  echo "错误: 正式发布只能从 main 分支触发" >&2
  exit 1
fi
if [ -n "$(git status --porcelain)" ]; then
  echo "错误: 工作区不干净，请先提交或清理改动" >&2
  exit 1
fi

current="$(sed -nE \
  's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' \
  CMakeLists.txt | head -1)"
if [ "$current" != "$version" ]; then
  echo "错误: CMakeLists.txt 版本（${current}）与目标版本（${version}）不一致" >&2
  exit 1
fi

git fetch origin main --quiet
head_commit="$(git rev-parse HEAD)"
remote_commit="$(git rev-parse origin/main)"
if [ "$head_commit" != "$remote_commit" ]; then
  echo "错误: 当前提交 ${head_commit} 与 origin/main ${remote_commit} 不一致" >&2
  exit 1
fi

tag="v${version}"
if git tag -l "$tag" | grep -q .; then
  echo "错误: 本地标签 ${tag} 已存在" >&2
  exit 1
fi
remote_tag="$(git ls-remote --tags origin "refs/tags/${tag}")"
if [ -n "$remote_tag" ]; then
  echo "错误: 远端标签 ${tag} 已存在" >&2
  exit 1
fi

known_runs="$(gh run list --workflow Release --branch main --commit "$head_commit" \
  --event workflow_dispatch --limit 20 --json databaseId --jq '.[].databaseId')"
dispatch_output="$(gh workflow run release.yml --ref main -f "tag=${tag}")"
printf '%s\n' "$dispatch_output"

run_id="$(printf '%s\n' "$dispatch_output" | \
  sed -nE 's#^.*/actions/runs/([0-9]+).*$#\1#p' | tail -1)"
for _ in $(seq 1 30); do
  [ -n "$run_id" ] && break
  sleep 2
  while IFS= read -r candidate; do
    if ! grep -qxF "$candidate" <<<"$known_runs"; then
      run_id="$candidate"
      break
    fi
  done < <(gh run list --workflow Release --branch main --commit "$head_commit" \
    --event workflow_dispatch --limit 20 --json databaseId --jq '.[].databaseId')
done
if [ -z "$run_id" ]; then
  echo "错误: 工作流已触发，但无法确定运行 ID；请运行 gh run list --workflow Release" >&2
  exit 1
fi

echo "Release 运行 ID: ${run_id}"
if [ "$wait_for_run" -eq 1 ]; then
  gh run watch "$run_id" --exit-status
fi
