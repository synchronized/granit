<#
.SYNOPSIS
  校验版本一致后创建并推送 v<version> 标签。
.DESCRIPTION
  校验 CMakeLists.txt 版本与目标版本一致、且标签不存在后，创建并推送 v<version>。
.EXAMPLE
  ./scripts/tag.ps1 0.26.0
#>
param(
  [Parameter(Mandatory = $true, Position = 0)]
  [string]$Version
)

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
  Write-Host "错误: 版本号格式应为 x.y.z，例如 0.26.0" -ForegroundColor Red
  exit 1
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

$cmakeContent = Get-Content -Raw -Encoding UTF8 CMakeLists.txt
if ($cmakeContent -match '(?m)^\s*VERSION\s+(\d+\.\d+\.\d+)') {
  $current = $Matches[1]
} else {
  Write-Host "错误: 无法从 CMakeLists.txt 读取当前版本" -ForegroundColor Red
  exit 1
}
if ($current -ne $Version) {
  Write-Host "错误: CMakeLists.txt 版本（$current）与目标（$Version）不一致，请先运行 scripts/release.ps1 $Version" -ForegroundColor Red
  exit 1
}

$existingTag = git tag -l "v$Version"
if ($existingTag) {
  Write-Host "错误: 标签 v$Version 已存在" -ForegroundColor Red
  exit 1
}

git tag "v$Version"
git push origin main "v$Version"
Write-Host "已创建并推送标签 v$Version"
