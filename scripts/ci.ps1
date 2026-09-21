<#
.SYNOPSIS
  用 gh CLI 触发对应 workflow。
.EXAMPLE
  ./scripts/ci.ps1 linux
  ./scripts/ci.ps1 quick-check
#>
param(
  [Parameter(Mandatory = $true, Position = 0)]
  [string]$Workflow
)

$workflows = @{
  'linux' = 'linux.yml'
  'windows' = 'windows.yml'
  'emscripten' = 'emscripten.yml'
  'quick-check' = 'quick-check.yml'
  'documentation' = 'documentation.yml'
  'release' = 'release.yml'
  'package-shader-toolchain' = 'package-shader-toolchain.yml'
}

if (-not $workflows.ContainsKey($Workflow)) {
  Write-Host "错误: 未知 workflow '$Workflow'" -ForegroundColor Red
  Write-Host "可用: $($workflows.Keys -join ' ')" -ForegroundColor Yellow
  exit 1
}

$wf = $workflows[$Workflow]
gh workflow run $wf
Write-Host "已触发 $wf"
