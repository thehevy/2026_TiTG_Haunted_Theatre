<#
.SYNOPSIS
    Compile, upload, and monitor the haunted-theatre sketches with arduino-cli.

.DESCRIPTION
    Sketch names, board FQBNs, board cores, and libraries are defined in sketches.json
    next to this script. Run "list" to see every sketch name.

.EXAMPLE
    .\tools\arduino\arduino.ps1 setup
    .\tools\arduino\arduino.ps1 list
    .\tools\arduino\arduino.ps1 ports
    .\tools\arduino\arduino.ps1 compile Arduino101_Node
    .\tools\arduino\arduino.ps1 compile all
    .\tools\arduino\arduino.ps1 upload Arduino101_Node -Port COM5
    .\tools\arduino\arduino.ps1 monitor -Port COM5
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('list', 'ports', 'compile', 'upload', 'monitor', 'setup')]
    [string]$Action = 'list',

    [Parameter(Position = 1)]
    [string]$Sketch,

    [string]$Port,

    # Overrides the FQBN from sketches.json, e.g. arduino:avr:nano:cpu=atmega328old for old-bootloader Nano clones.
    [string]$Fqbn,

    [int]$Baud = 115200,

    # Used only by "setup". Pass an empty string to clear the arduino-cli proxy.
    [string]$Proxy = $env:HTTPS_PROXY
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$config = Get-Content (Join-Path $PSScriptRoot 'sketches.json') -Raw | ConvertFrom-Json

function Get-ArduinoCli {
    $cmd = Get-Command arduino-cli -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    $fallback = Join-Path $env:LOCALAPPDATA 'Programs\arduino-cli\arduino-cli.exe'
    if (Test-Path $fallback) { return $fallback }
    throw 'arduino-cli not found. Install it to %LOCALAPPDATA%\Programs\arduino-cli (see SETUP_GUIDE.md).'
}

function Invoke-Cli {
    param([string[]]$CliArgs)
    Write-Host "> arduino-cli $($CliArgs -join ' ')" -ForegroundColor DarkGray
    # ESP cores write size reports to stderr; PowerShell 5.1 would treat redirected stderr as a terminating error.
    $ErrorActionPreference = 'Continue'
    & $script:cli @CliArgs 2>&1 | ForEach-Object { "$_" }
    if ($LASTEXITCODE -ne 0) { throw "arduino-cli exited with code $LASTEXITCODE" }
}

function Get-SketchEntry {
    param([string]$Name)
    if (-not $Name) { throw 'Specify a sketch name. Run "list" to see the available names.' }
    $prop = $config.sketches.PSObject.Properties | Where-Object { $_.Name -eq $Name }
    if (-not $prop) { throw "Unknown sketch '$Name'. Run ""list"" to see the available names." }
    [pscustomobject]@{
        Name = $prop.Name
        Path = Join-Path $repoRoot $prop.Value.path
        Fqbn = $(if ($Fqbn) { $Fqbn } else { $prop.Value.fqbn })
    }
}

function Find-Port {
    param([string]$BoardFqbn)
    $platform = ($BoardFqbn -split ':')[0..2] -join ':'
    $ports = (& $script:cli board list --format json | ConvertFrom-Json).detected_ports
    $match = @($ports | Where-Object { $_.matching_boards | Where-Object { $_.fqbn -eq $platform } })
    if ($match.Count -eq 1) { return $match[0].port.address }
    $list = ($ports | ForEach-Object { "  $($_.port.address)  $($_.port.label)" }) -join "`n"
    throw "Could not auto-detect a single port for $BoardFqbn. Pass -Port COMx. Detected ports:`n$list"
}

$script:cli = Get-ArduinoCli

switch ($Action) {
    'list' {
        $config.sketches.PSObject.Properties |
            ForEach-Object { [pscustomobject]@{ Sketch = $_.Name; Fqbn = $_.Value.fqbn; Path = $_.Value.path } } |
            Format-Table -AutoSize
    }

    'ports' {
        Invoke-Cli @('board', 'list')
    }

    'setup' {
        $urls = @($config.additionalUrls)
        Invoke-Cli (@('config', 'set', 'board_manager.additional_urls') + $urls)
        if ($Proxy) { Invoke-Cli @('config', 'set', 'network.proxy', $Proxy) }
        else { & $script:cli config delete network.proxy 2>$null }
        Invoke-Cli @('config', 'set', 'network.connection_timeout', '600s')
        Invoke-Cli @('core', 'update-index')
        foreach ($core in $config.cores) { Invoke-Cli @('core', 'install', $core) }
        Invoke-Cli (@('lib', 'install') + @($config.libraries))
        Invoke-Cli @('core', 'list')
    }

    'compile' {
        $names = if ($Sketch -eq 'all') { @($config.sketches.PSObject.Properties.Name) } else { @($Sketch) }
        $failed = @()
        foreach ($name in $names) {
            $entry = Get-SketchEntry $name
            Write-Host "`n=== Compiling $($entry.Name) [$($entry.Fqbn)]" -ForegroundColor Cyan
            try { Invoke-Cli @('compile', '--fqbn', $entry.Fqbn, '--warnings', 'default', $entry.Path) }
            catch { $failed += $entry.Name; Write-Host $_ -ForegroundColor Red }
        }
        if ($failed.Count -gt 0) {
            Write-Host "`nFAILED: $($failed -join ', ')" -ForegroundColor Red
            exit 1
        }
        Write-Host "`nAll $($names.Count) sketch(es) compiled." -ForegroundColor Green
    }

    'upload' {
        $entry = Get-SketchEntry $Sketch
        if (-not $Port) { $Port = Find-Port $entry.Fqbn }
        if ($entry.Fqbn -like 'Intel:arc32:*') {
            Write-Host 'Arduino 101: close any Serial Monitor first. If upload stalls waiting for the device, press MASTER RESET right as the upload starts.' -ForegroundColor Yellow
        }
        elseif ($entry.Fqbn -like 'esp32:*') {
            Write-Host 'ESP32: if upload stops at "Connecting...", hold BOOT until writing starts.' -ForegroundColor Yellow
        }
        elseif ($entry.Fqbn -eq 'esp8266:esp8266:generic') {
            Write-Host 'ESP-01: GPIO0 must be held LOW at reset to enter flash mode.' -ForegroundColor Yellow
        }
        Write-Host "=== Uploading $($entry.Name) [$($entry.Fqbn)] to $Port" -ForegroundColor Cyan
        Invoke-Cli @('compile', '--fqbn', $entry.Fqbn, '--upload', '--port', $Port, $entry.Path)
    }

    'monitor' {
        if (-not $Port) {
            if (-not $Sketch) { throw 'Pass -Port COMx (or a sketch name to auto-detect the port).' }
            $Port = Find-Port (Get-SketchEntry $Sketch).Fqbn
        }
        Invoke-Cli @('monitor', '--port', $Port, '--config', "baudrate=$Baud")
    }
}
