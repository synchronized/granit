# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

<#
.SYNOPSIS
  在当前 main 提交上触发并等待 Granit Release 工作流。
.EXAMPLE
  ./scripts/publish.ps1 0.27.0
.EXAMPLE
  ./scripts/publish.ps1 0.27.0 -NoWait
#>
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true, Position = 0)]
  [string]$Version,
  [switch]$NoWait
)

$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
  throw '版本号格式应为 x.y.z，例如 0.27.0'
}
if (-not (Get-Command gh -ErrorAction SilentlyContinue)) {
  throw '未找到 gh CLI'
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

if ((git branch --show-current) -ne 'main') {
  throw '正式发布只能从 main 分支触发'
}
if (git status --porcelain) {
  throw '工作区不干净，请先提交或清理改动'
}

$versionContent = Get-Content -Raw -Encoding UTF8 cmake/granit_version.cmake
if ($versionContent -match '(?m)^set\(GRANIT_PROJECT_VERSION "(\d+\.\d+\.\d+)"\)$') {
  $currentVersion = $Matches[1]
} else {
  throw '无法从 cmake/granit_version.cmake 读取当前版本'
}
if ($currentVersion -ne $Version) {
  throw "工程版本（$currentVersion）与目标版本（$Version）不一致"
}

git fetch origin main --quiet
if ($LASTEXITCODE -ne 0) {
  throw '无法更新 origin/main'
}
$headCommit = git rev-parse HEAD
$remoteCommit = git rev-parse origin/main
if ($headCommit -ne $remoteCommit) {
  throw "当前提交 $headCommit 与 origin/main $remoteCommit 不一致"
}

$tag = "v$Version"
if (git tag -l $tag) {
  throw "本地标签 $tag 已存在"
}
$remoteTag = git ls-remote --tags origin "refs/tags/$tag"
if ($LASTEXITCODE -ne 0) {
  throw '无法查询远端标签'
}
if ($remoteTag) {
  throw "远端标签 $tag 已存在"
}

$knownRuns = @(gh run list --workflow Release --branch main --commit $headCommit `
    --event workflow_dispatch --limit 20 --json databaseId --jq '.[].databaseId')
$dispatchOutput = gh workflow run release.yml --ref main -f "tag=$tag"
if ($LASTEXITCODE -ne 0) {
  throw '触发 Release 工作流失败'
}
$dispatchOutput | Write-Host

$runId = $null
foreach ($line in $dispatchOutput) {
  if ($line -match '/actions/runs/(\d+)') {
    $runId = $Matches[1]
  }
}
for ($attempt = 0; -not $runId -and $attempt -lt 30; ++$attempt) {
  Start-Sleep -Seconds 2
  $currentRuns = @(gh run list --workflow Release --branch main --commit $headCommit `
      --event workflow_dispatch --limit 20 --json databaseId --jq '.[].databaseId')
  $runId = $currentRuns | Where-Object { $_ -notin $knownRuns } | Select-Object -First 1
}
if (-not $runId) {
  throw '工作流已触发，但无法确定运行 ID；请使用 gh run list --workflow Release 查看'
}

Write-Host "Release 运行 ID: $runId"
if (-not $NoWait) {
  gh run watch $runId --exit-status
  if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
  }
}
