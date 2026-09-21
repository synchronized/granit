<#
.SYNOPSIS
  把根 CMakeLists.txt、README、CHANGELOG 的版本号升级到指定版本。

.DESCRIPTION
  只修改文件并打印 diff，不执行 git commit / tag / push —— 由维护者确认后手动执行。
  与 scripts/release.sh 功能等价，供 Windows PowerShell 环境使用。

.EXAMPLE
  ./scripts/release.ps1 0.26.0
#>
param(
  [string]$NewVersion = ''
)

# 用法提示
if ([string]::IsNullOrEmpty($NewVersion)) {
  Write-Host "用法: scripts/release.ps1 <new-version>" -ForegroundColor Yellow
  Write-Host "示例: scripts/release.ps1 0.26.0" -ForegroundColor Yellow
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
git diff --quiet
$diffExit = $LASTEXITCODE
git diff --cached --quiet
$cachedExit = $LASTEXITCODE
if ($diffExit -ne 0 -or $cachedExit -ne 0) {
  Write-Host "错误: 工作区有未提交改动，请先提交或暂存" -ForegroundColor Red
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
Write-Host "确认后执行："
Write-Host "  git add CMakeLists.txt README.md CHANGELOG.md"
Write-Host "  git commit -m `"chore: 发布 $NewVersion`""
Write-Host "  git push origin main"
Write-Host "  .\scripts\publish.ps1 $NewVersion"
Write-Host "完整流程见 docs/guides/release.md"
