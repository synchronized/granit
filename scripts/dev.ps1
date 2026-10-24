# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Granit contributors

<#
.SYNOPSIS
  配置、构建并按范围验证，日志保存至 build/validation。
#>
param(
  [Parameter(Mandatory = $true, Position = 0)][string]$Preset,
  [switch]$Test,
  [ValidateSet('default', 'quick', 'stress')][string]$Suite = 'default',
  [ValidateRange(1, 86400)][int]$Seconds = 3600,
  [uint32]$Seed = 20261024
)
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ($Test -or $Suite -ne 'default') {
  & cmake "-DGRANIT_SOURCE_DIR=$repoRoot" "-DGRANIT_PRESET=$Preset" `
    "-DGRANIT_SUITE=$Suite" "-DGRANIT_STRESS_SECONDS=$Seconds" `
    "-DGRANIT_STRESS_SEED=$Seed" -P "$PSScriptRoot/validate.cmake"
  exit $LASTEXITCODE
}
Push-Location $repoRoot
try {
  cmake --preset $Preset
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  cmake --build --preset $Preset
  exit $LASTEXITCODE
} finally { Pop-Location }
