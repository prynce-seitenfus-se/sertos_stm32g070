<#
.SYNOPSIS
    Launch the SerTOS STM32G070 firmware inside Renode.

.DESCRIPTION
    Thin wrapper around renode.exe that:
      * Verifies the Renode installation and the ELF for the requested build.
      * Hands off to the project's Renode script (renode\sertos_stm32g070.resc).
      * Supports GUI and headless/console-only modes.

.PARAMETER Config
    CMake build directory under build\ to run. Defaults to Debug.

.PARAMETER RenodePath
    Full path to renode.exe. Defaults to C:\renode\1.17.0\renode.exe.

.PARAMETER Headless
    Run without the GUI (no UART analyzer window). The Renode monitor
    stays attached to the current console; type `quit` to exit.

.PARAMETER Elf
    Override the ELF path entirely. Takes precedence over -Config.

.PARAMETER ProfilerSocketPort
    Expose USART2 through a Renode TCP socket terminal on this port. Use
    capture-profiler-dump.ps1 from another terminal to request and save a PROF-BIN dump.
    Zero keeps the normal UART analyzer mode.

.PARAMETER Extra
    Extra arguments forwarded verbatim to renode.exe.

.EXAMPLE
    .\renode\run.ps1
    # Debug build, GUI, UART window.

.EXAMPLE
    .\renode\run.ps1 -Config Release -Headless

.EXAMPLE
    .\renode\run.ps1 -Elf C:\tmp\other.elf -RenodePath 'C:\renode\1.17.0\renode.exe'

.EXAMPLE
    .\renode\run.ps1 -Config Renode -ProfilerSocketPort 3456
#>
[CmdletBinding()]
param(
    [string]   $Config     = 'Debug',
    [string]   $RenodePath = 'C:\renode\1.17.0\renode.exe',
    [switch]   $Headless,
    [string]   $Elf,
    [ValidateRange(0, 65535)]
    [int]      $ProfilerSocketPort = 0,
    [string[]] $Extra
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
if (($ProfilerSocketPort -ne 0) -and ($Config -ne 'Renode')) {
    throw "Profiler socket mode requires the Renode build preset. Configure and build with " +
          "'cmake --preset Renode' and 'cmake --build --preset Renode', then pass -Config Renode."
}

if ($ProfilerSocketPort -eq 0) {
    $rescScript = Join-Path $PSScriptRoot 'sertos_stm32g070.resc'
} else {
    $rescScript = Join-Path $PSScriptRoot 'sertos_stm32g070_profiler_socket.resc'
}

if (-not (Test-Path $RenodePath)) {
    throw "Renode not found at '$RenodePath'. Pass -RenodePath to override."
}
if (-not (Test-Path $rescScript)) {
    throw "Renode script missing: $rescScript"
}

if (-not $Elf) {
    $Elf = Join-Path $projectRoot "build\$Config\sertos_stm32g070.elf"
}
if (-not (Test-Path $Elf)) {
    throw "Firmware ELF not found: $Elf`n" +
          "Build it first, e.g. `cmake --build build\$Config`."
}
$Elf = (Resolve-Path $Elf).Path

Write-Host "Renode     : $RenodePath"
Write-Host "Script     : $rescScript"
Write-Host "Firmware   : $Elf"
Write-Host "Mode       : $(if ($Headless) { 'headless' } else { 'GUI' })"
if ($ProfilerSocketPort -ne 0) {
    Write-Host "Profiler TCP: 127.0.0.1:$ProfilerSocketPort"
}
Write-Host ''

# $bin is read by the selected .resc script via `$bin ?= ...`.
# Renode's monitor uses @ for file paths and ; to separate commands.
$monitorCmd = "`$bin=@$Elf"
if ($ProfilerSocketPort -ne 0) {
    $monitorCmd += "; `$profilerPort=$ProfilerSocketPort"
}
$monitorCmd += "; include @$rescScript"

$args = @('-e', $monitorCmd)
if ($Headless) {
    $args = @('--console', '--disable-xwt', '--plain') + $args
}
if ($Extra) {
    $args += $Extra
}

& $RenodePath @args
exit $LASTEXITCODE
