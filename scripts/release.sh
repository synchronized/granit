#!/usr/bin/env bash
set -euo pipefail

# 用法: scripts/release.sh <new-version>
#
# 把根 CMakeLists.txt、README、CHANGELOG 的版本号升级到 <new-version>。
# 只修改文件并打印 diff，不执行 git commit / tag / push —— 由维护者确认后手动执行。
#
# 示例:
#   scripts/release.sh 0.26.0

if [ $# -ne 1 ]; then
  echo "用法: scripts/release.sh <new-version>" >&2
  echo "示例: scripts/release.sh 0.26.0" >&2
  exit 1
fi
new_version="$1"

if ! echo "$new_version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "错误: 版本号格式应为 x.y.z，例如 0.26.0" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if ! git diff --quiet || ! git diff --cached --quiet; then
  echo "错误: 工作区有未提交改动，请先提交或暂存" >&2
  exit 1
fi

# 从根 CMakeLists.txt 读取当前版本（行首的 project VERSION）。
old_version="$(sed -nE 's/^[[:space:]]*VERSION[[:space:]]+([0-9]+\.[0-9]+\.[0-9]+).*/\1/p' CMakeLists.txt | head -1)"
if [ -z "$old_version" ]; then
  echo "错误: 无法从 CMakeLists.txt 读取当前版本" >&2
  exit 1
fi
if [ "$old_version" = "$new_version" ]; then
  echo "错误: 新版本与当前版本相同（${new_version}）" >&2
  exit 1
fi

old_version_escaped="$(printf '%s' "$old_version" | sed 's/[.]/\\./g')"
date_today="$(date +%F)"

echo "从 ${old_version} 升级到 ${new_version}（日期 ${date_today}）"
echo ""

# 1. 根 CMakeLists.txt：project(VERSION)
sed -i "s/^\([[:space:]]*VERSION[[:space:]]\+\)[0-9.]*/\1${new_version}/" CMakeLists.txt

# 2. README.md：最新版本号与 release 链接 tag
sed -i "s/${old_version_escaped}/${new_version}/g" README.md

# 3. CHANGELOG.md：在 Unreleased 下插入带日期的新版本章节
sed -i "s/^## Unreleased$/## Unreleased\n\n## ${new_version} - ${date_today}/" CHANGELOG.md

echo "改动如下（确认无误后提交，再触发发布工作流）："
git diff --stat
echo ""
git diff -- CMakeLists.txt README.md CHANGELOG.md
echo ""
echo "确认后执行："
echo "  git add CMakeLists.txt README.md CHANGELOG.md"
echo "  git commit -m \"chore: 发布 ${new_version}\""
echo "  git push origin main"
echo "  bash scripts/publish.sh ${new_version}"
echo "完整流程见 docs/guides/release.md"
