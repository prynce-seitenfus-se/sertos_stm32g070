<#
.SYNOPSIS
    Request a profiler dump from a G070 Renode session and save the PROF-BIN packet.

.DESCRIPTION
    Connects to the TCP socket terminal created by run.ps1 -ProfilerSocketPort, sends
    the profiler command, skips preceding USART2 text, and saves one complete binary dump.

.PARAMETER Port
    TCP port selected when starting Renode. Defaults to 3456.

.PARAMETER OutputFile
    Destination file for the raw PROF-BIN v2 packet.

.PARAMETER TimeoutSeconds
    Maximum time to wait for the complete response.

.EXAMPLE
    .\renode\capture-profiler-dump.ps1 -Port 3456 -OutputFile .\build\renode-profiler.bin
#>
[CmdletBinding()]
param(
    [ValidateRange(1, 65535)]
    [int] $Port = 3456,
    [string] $OutputFile = 'renode-profiler.bin',
    [ValidateRange(1, 600)]
    [int] $TimeoutSeconds = 30
)

$ErrorActionPreference = 'Stop'

function Read-ExactBytes
{
    param(
        [System.Net.Sockets.NetworkStream] $NetworkStream,
        [int] $Length
    )

    $buffer = New-Object byte[] $Length
    $offset = 0

    while ($offset -lt $Length) {
        $read = $NetworkStream.Read($buffer, $offset, $Length - $offset)
        if ($read -le 0) {
            throw 'The Renode socket closed before the profiler packet was complete.'
        }
        $offset += $read
    }

    return ,$buffer
}

$client = New-Object System.Net.Sockets.TcpClient
try {
    $client.Connect('127.0.0.1', $Port)
    $networkStream = $client.GetStream()
    $networkStream.ReadTimeout = $TimeoutSeconds * 1000
    $networkStream.WriteTimeout = $TimeoutSeconds * 1000

    $command = [System.Text.Encoding]::ASCII.GetBytes("prof-dump`r`n")
    $networkStream.Write($command, 0, $command.Length)
    $networkStream.Flush()

    $magicWindow = New-Object 'System.Collections.Generic.Queue[byte]'
    $magicFound = $false
    while (-not $magicFound) {
        $nextByte = $networkStream.ReadByte()
        if ($nextByte -lt 0) {
            throw 'The Renode socket closed before the profiler dump started.'
        }

        if ($magicWindow.Count -eq 4) {
            $null = $magicWindow.Dequeue()
        }
        $magicWindow.Enqueue([byte]$nextByte)

        if ($magicWindow.Count -eq 4) {
            $window = $magicWindow.ToArray()
            $magicFound = ($window[0] -eq 0x50) -and
                          ($window[1] -eq 0x52) -and
                          ($window[2] -eq 0x4F) -and
                          ($window[3] -eq 0x46)
        }
    }

    $headerTail = Read-ExactBytes -NetworkStream $networkStream -Length 12
    $header = [byte[]](@(0x50, 0x52, 0x4F, 0x46) + $headerTail)
    $version = [System.BitConverter]::ToUInt16($header, 4)
    $recordCount = [System.BitConverter]::ToUInt16($header, 6)
    $headerLength = switch ($version) { 2 { 16 } 3 { 24 } 4 { 28 } default { 0 } }
    if (($headerLength -eq 0) -or ($recordCount -gt 64)) {
        throw "Invalid profiler header (version $version, records $recordCount)."
    }

    $packetLength = $headerLength + (24 * $recordCount) + 4
    $packetTail = Read-ExactBytes -NetworkStream $networkStream -Length ($packetLength - 16)
    $packet = [byte[]]($header + $packetTail)

    if ([System.IO.Path]::IsPathRooted($OutputFile)) {
        $fullOutputPath = $OutputFile
    } else {
        $fullOutputPath = [System.IO.Path]::GetFullPath(
            (Join-Path -Path (Get-Location).ProviderPath -ChildPath $OutputFile)
        )
    }
    $outputDirectory = [System.IO.Path]::GetDirectoryName($fullOutputPath)
    if (-not [System.IO.Directory]::Exists($outputDirectory)) {
        [void][System.IO.Directory]::CreateDirectory($outputDirectory)
    }
    [System.IO.File]::WriteAllBytes($fullOutputPath, $packet)

    Write-Host "Saved PROF-BIN v$version dump: $fullOutputPath"
    Write-Host "Records: $recordCount; bytes: $packetLength"
}
finally {
    if ($null -ne $client) {
        $client.Dispose()
    }
}
