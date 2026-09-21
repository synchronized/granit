#!/usr/bin/env bash
set -euo pipefail

# 用法: scripts/tag.sh <version>
# 校验 CMakeLists.txt 版本与 tag 一致性后，创建并推送 v<version> 标签。
#
# 示例:
#   scripts/tag.sh 0.26.0

if [ $# -ne 1 ]; then
  echo "用法: scripts/tag.sh <version>" >&2
  echo "示例: scripts/tag.sh 0.26.0" >&2
  exit 1
fi
version="$1"

if ! echo "$version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "错误: 版本号格式应为 x.y.z，例如 0.26.0" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

current="$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt | head -1)"
if [ "$current" != "$version" ]; then
  echo "错误: CMakeLists.txt 版本（${current}）与目标（${version}）不一致，请先运行 scripts/release.sh ${version}" >&2
  exit 1
fi

if git tag -l "v${version}" | grep -q .; then
  echo "错误: 标签 v${version} 已存在" >&2
  exit 1
fi

git tag "v${version}"
git push origin main "v${version}"
echo "已创建并推送标签 v${version}"
