<#
.SYNOPSIS
    Runs the sewcet WCET tools for this project.
.DESCRIPTION
    Thin wrapper around <sewcet>/tools/run_wcet.ps1. The sewcet workspace is
    $env:SEWCET_DIR, or ..\..\sewcet next to this repository.
    -Config selects the project config: wcet_targets.json (instrumented Debug
    build, default) or wcet_measure.json (uninstrumented Measure build with
    explicit profiler scopes).
.EXAMPLE
    .\sewcet\run_wcet.ps1
    .\sewcet\run_wcet.ps1 -Model generic
    .\sewcet\run_wcet.ps1 -Elf .\build\Release\sertos_stm32g070.elf
    .\sewcet\run_wcet.ps1 -Config wcet_measure.json
#>
[CmdletBinding()]
param(
    [string]$Config = 'wcet_targets.json',
    [string]$Elf,
    [ValidateSet('m0plus', 'trivial', 'generic')]
    [string]$Model
)

$ErrorActionPreference = 'Stop'

$sewcet = if ($env:SEWCET_DIR) { $env:SEWCET_DIR } else { Join-Path $PSScriptRoot '..\..\sewcet' }
$tool = Join-Path $sewcet 'tools\run_wcet.ps1'
if (-not (Test-Path -LiteralPath $tool)) {
    throw "sewcet tools not found: $tool (set SEWCET_DIR)"
}

$configPath = if ([System.IO.Path]::IsPathRooted($Config)) { $Config } else { Join-Path $PSScriptRoot $Config }
$toolArgs = @{ Config = $configPath }
if ($Elf) { $toolArgs.Elf = $Elf }
if ($Model) { $toolArgs.Model = $Model }

& $tool @toolArgs
exit $LASTEXITCODE
