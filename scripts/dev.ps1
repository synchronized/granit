<#
.SYNOPSIS
  配置并构建指定 preset，可选运行测试。
.DESCRIPTION
  等价于: cmake --preset <preset> && cmake --build --preset <preset> [&& ctest --preset <preset>]
.EXAMPLE
  ./scripts/dev.ps1 linux-clang-release -Test
#>
param(
  [Parameter(Mandatory = $true, Position = 0)]
  [string]$Preset,
  [switch]$Test
)

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Set-Location $repoRoot

cmake --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $Preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if ($Test) {
  ctest --preset $Preset --output-on-failure
}
