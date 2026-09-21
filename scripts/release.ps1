<#
.SYNOPSIS
  把根 CMakeLists.txt、README、CHANGELOG 的版本号升级到指定版本。

.DESCRIPTION
  默认只修改文件并打印 diff；指定 -Commit 时校验改动并创建版本提交。
  脚本不会创建标签、推送分支或发布 Release。
  与 scripts/release.sh 功能等价，供 Windows PowerShell 环境使用。

.EXAMPLE
  ./scripts/release.ps1 0.26.0
.EXAMPLE
  ./scripts/release.ps1 0.26.0 -Commit
#>
[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [string]$NewVersion = '',
  [switch]$Commit
)

# 用法提示
if ([string]::IsNullOrEmpty($NewVersion)) {
  Write-Host "用法: scripts/release.ps1 <new-version> [-Commit]" -ForegroundColor Yellow
  Write-Host "示例: scripts/release.ps1 0.26.0 -Commit" -ForegroundColor Yellow
  exit 1
}

# 校验版本号格式 x.y.z
if ($NewVersion -notmatch '^\d+\.\d+\.\d+$') {
  Write-Host "错误: 版本号格式应为 x.y.z，例如 0.26.0" -ForegroundColor Red
  exit 1
}

# 定位仓库根
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

# 校验 git 状态干净
if (git status --porcelain) {
  Write-Host "错误: 工作区有未提交或未跟踪的改动，请先处理" -ForegroundColor Red
  exit 1
}

# 从根 CMakeLists.txt 读取当前版本（行首的 project VERSION）
$cmakeContent = Get-Content -Raw -Encoding UTF8 CMakeLists.txt
if ($cmakeContent -match '(?m)^\s*VERSION\s+(\d+\.\d+\.\d+)') {
  $oldVersion = $Matches[1]
} else {
  Write-Host "错误: 无法从 CMakeLists.txt 读取当前版本" -ForegroundColor Red
  exit 1
}
if ($oldVersion -eq $NewVersion) {
  Write-Host "错误: 新版本与当前版本相同（$NewVersion）" -ForegroundColor Red
  exit 1
}

$dateToday = Get-Date -Format 'yyyy-MM-dd'
Write-Host "从 $oldVersion 升级到 $NewVersion（日期 $dateToday）"
Write-Host ""

# 1. 根 CMakeLists.txt：project(VERSION)
$replacement = '${1}' + $NewVersion
$cmakeContent = $cmakeContent -replace '(?m)^(\s*VERSION\s+)[\d.]+', $replacement
Set-Content -Path CMakeLists.txt -Value $cmakeContent -Encoding UTF8 -NoNewline

# 2. README.md：最新版本号与 release 链接 tag
$readmeContent = Get-Content -Raw -Encoding UTF8 README.md
$readmeContent = $readmeContent -replace [regex]::Escape($oldVersion), $NewVersion
Set-Content -Path README.md -Value $readmeContent -Encoding UTF8 -NoNewline

# 3. CHANGELOG.md：在 Unreleased 下插入带日期的新版本章节
$changelogContent = Get-Content -Raw -Encoding UTF8 CHANGELOG.md
$changelogContent = $changelogContent -replace '(?m)^## Unreleased$', "## Unreleased`n`n## $NewVersion - $dateToday"
Set-Content -Path CHANGELOG.md -Value $changelogContent -Encoding UTF8 -NoNewline

Write-Host "改动如下（确认无误后提交，再触发发布工作流）："
git diff --stat
Write-Host ""
git diff -- CMakeLists.txt README.md CHANGELOG.md
Write-Host ""
if ($Commit) {
  git diff --check
  if ($LASTEXITCODE -ne 0) {
    throw '版本文件存在空白或格式错误'
  }
  cmake "-DGRANIT_SOURCE_DIR=$repoRoot" "-DGRANIT_RELEASE_TAG=v$NewVersion" `
    -P tests/packaging/check_release_version.cmake
  if ($LASTEXITCODE -ne 0) {
    throw '发布版本校验失败'
  }
  git add -- CMakeLists.txt README.md CHANGELOG.md
  git commit -m "chore: 发布 $NewVersion"
  if ($LASTEXITCODE -ne 0) {
    throw '创建版本提交失败'
  }
  Write-Host "已创建版本提交：$(git rev-parse --short HEAD)"
} else {
  Write-Host "如需自动创建版本提交，重新还原后执行："
  Write-Host "  .\scripts\release.ps1 $NewVersion -Commit"
  Write-Host "也可以检查当前改动后手动提交。"
}
Write-Host ""
Write-Host "版本提交合并并推送到 main 后执行："
Write-Host "  git switch main"
Write-Host "  git pull --ff-only"
Write-Host "  .\scripts\publish.ps1 $NewVersion"
Write-Host "完整流程见 docs/guides/release.md"
