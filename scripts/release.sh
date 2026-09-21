#!/usr/bin/env bash
set -euo pipefail

# 用法: scripts/release.sh <new-version> [--commit]
#
# 把工程版本文件、README、CHANGELOG 升级到 <new-version>。
# 默认只修改文件并打印 diff；指定 --commit 时校验改动并创建版本提交。
# 不创建标签、推送分支或发布 Release。
#
# 示例:
#   bash scripts/release.sh 0.26.0 --commit

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
  echo "用法: scripts/release.sh <new-version> [--commit]" >&2
  echo "示例: scripts/release.sh 0.26.0 --commit" >&2
  exit 1
fi
new_version="$1"
commit_changes=0
if [ $# -eq 2 ]; then
  if [ "$2" != "--commit" ]; then
    echo "错误: 未知参数 $2" >&2
    exit 1
  fi
  commit_changes=1
fi

if ! echo "$new_version" | grep -Eq '^[0-9]+\.[0-9]+\.[0-9]+$'; then
  echo "错误: 版本号格式应为 x.y.z，例如 0.26.0" >&2
  exit 1
fi

repo_root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$repo_root"

if [ -n "$(git status --porcelain)" ]; then
  echo "错误: 工作区有未提交或未跟踪的改动，请先处理" >&2
  exit 1
fi

# 从唯一版本文件读取当前版本。
version_file="cmake/granit_version.cmake"
old_version="$(sed -nE \
  's/^set\(GRANIT_PROJECT_VERSION "([0-9]+\.[0-9]+\.[0-9]+)"\)$/\1/p' \
  "$version_file" | head -1)"
if [ -z "$old_version" ]; then
  echo "错误: 无法从 ${version_file} 读取当前版本" >&2
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

# 1. 唯一工程版本文件
sed -i \
  "s/^set(GRANIT_PROJECT_VERSION \"[0-9.]*\")$/set(GRANIT_PROJECT_VERSION \"${new_version}\")/" \
  "$version_file"

# 2. README.md：最新版本号与 release 链接 tag
sed -i "s/${old_version_escaped}/${new_version}/g" README.md

# 3. CHANGELOG.md：在 Unreleased 下插入带日期的新版本章节
sed -i "s/^## Unreleased$/## Unreleased\n\n## ${new_version} - ${date_today}/" CHANGELOG.md

echo "改动如下（确认无误后提交，再触发发布工作流）："
git diff --stat
echo ""
git diff -- "$version_file" README.md CHANGELOG.md
echo ""
if [ "$commit_changes" -eq 1 ]; then
  git diff --check
  cmake -DGRANIT_SOURCE_DIR="$repo_root" -DGRANIT_RELEASE_TAG="v${new_version}" \
    -P tests/packaging/check_release_version.cmake
  git add -- "$version_file" README.md CHANGELOG.md
  git commit -m "chore: 发布 ${new_version}"
  echo "已创建版本提交：$(git rev-parse --short HEAD)"
else
  echo "如需自动创建版本提交，重新还原后执行："
  echo "  bash scripts/release.sh ${new_version} --commit"
  echo "也可以检查当前改动后手动提交。"
fi
echo ""
echo "版本提交合并并推送到 main 后执行："
echo "  git switch main"
echo "  git pull --ff-only"
echo "  bash scripts/publish.sh ${new_version}"
echo "完整流程见 docs/guides/release.md"
