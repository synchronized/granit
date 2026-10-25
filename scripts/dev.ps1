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
  [uint32]$Seed = 20261024,
  [uint32]$Iterations = 0
)
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ($Test -or $Suite -ne 'default') {
  $exitFile = New-TemporaryFile
  try {
    & cmake "-DGRANIT_SOURCE_DIR=$repoRoot" "-DGRANIT_PRESET=$Preset" `
      "-DGRANIT_SUITE=$Suite" "-DGRANIT_STRESS_SECONDS=$Seconds" `
      "-DGRANIT_STRESS_SEED=$Seed" "-DGRANIT_STRESS_ITERATIONS=$Iterations" `
      "-DGRANIT_EXIT_CODE_FILE=$($exitFile.FullName)" -P "$PSScriptRoot/validate.cmake"
    $code = $LASTEXITCODE
    $raw = ([string](Get-Content -LiteralPath $exitFile.FullName -Raw)).Trim()
    $parsed = 0
    if ([int]::TryParse($raw, [ref]$parsed) -and $parsed -ge 0) { $code = $parsed }
  } finally { Remove-Item -LiteralPath $exitFile.FullName }
  exit $code
}
Push-Location $repoRoot
try {
  cmake --preset $Preset
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
  cmake --build --preset $Preset
  exit $LASTEXITCODE
} finally { Pop-Location }
