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
    .\tools\arduino\arduino.ps1 intellisense
#>
[CmdletBinding()]
param(
    [Parameter(Position = 0)]
    [ValidateSet('list', 'ports', 'compile', 'upload', 'monitor', 'setup', 'intellisense')]
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

# Splits a gcc response file (@file) using gcc rules: whitespace separates, quotes group, backslash escapes.
function Split-ResponseText {
    param([string]$Text)
    $tokens = New-Object System.Collections.Generic.List[string]
    $sb = New-Object System.Text.StringBuilder
    $quote = [char]0
    $hasToken = $false
    for ($i = 0; $i -lt $Text.Length; $i++) {
        $c = $Text[$i]
        if ($c -eq '\' -and ($i + 1) -lt $Text.Length) { [void]$sb.Append($Text[++$i]); $hasToken = $true; continue }
        if ($quote -ne [char]0) {
            if ($c -eq $quote) { $quote = [char]0 } else { [void]$sb.Append($c) }
            continue
        }
        if ($c -eq '"' -or $c -eq "'") { $quote = $c; $hasToken = $true; continue }
        if ([char]::IsWhiteSpace($c)) {
            if ($hasToken) { $tokens.Add($sb.ToString()); [void]$sb.Clear(); $hasToken = $false }
            continue
        }
        [void]$sb.Append($c); $hasToken = $true
    }
    if ($hasToken) { $tokens.Add($sb.ToString()) }
    return , $tokens.ToArray()
}

# Expands @response files and rewrites -iprefix/-iwithprefixbefore as plain -I so the C/C++ extension can read them.
function Expand-CompilerArgs {
    param([string[]]$InputArgs)
    $out = New-Object System.Collections.Generic.List[string]
    $prefix = ''
    $queue = New-Object System.Collections.Generic.Queue[string]
    foreach ($a in $InputArgs) { $queue.Enqueue($a) }
    while ($queue.Count -gt 0) {
        $a = $queue.Dequeue()
        if ($a.StartsWith('@') -and (Test-Path -LiteralPath $a.Substring(1))) {
            $inner = Split-ResponseText (Get-Content -LiteralPath $a.Substring(1) -Raw)
            $rest = $queue.ToArray(); $queue.Clear()
            foreach ($x in @($inner) + @($rest)) { $queue.Enqueue($x) }
            continue
        }
        if ($a -eq '-iprefix') { $prefix = $queue.Dequeue(); continue }
        if ($a.StartsWith('-iprefix')) { $prefix = $a.Substring(8); continue }
        if ($a -eq '-iwithprefixbefore' -or $a -eq '-iwithprefix') { $out.Add('-I' + $prefix + $queue.Dequeue()); continue }
        if ($a.StartsWith('-iwithprefixbefore')) { $out.Add('-I' + $prefix + $a.Substring(18)); continue }
        $out.Add($a)
    }
    return , $out.ToArray()
}

function New-IntelliSenseEntry {
    param($Entry)
    $buildPath = Join-Path $repoRoot ".build\$($Entry.Name)"
    try {
        Invoke-Cli @('compile', '--fqbn', $Entry.Fqbn, '--only-compilation-database', '--build-path', $buildPath, $Entry.Path) | Out-Null
    }
    catch {
        # Some legacy cores (Intel Curie) run post-link hooks that fail when linking is skipped; a full build also writes the database.
        Write-Host 'Compile-database-only mode failed; falling back to a full compile.' -ForegroundColor Yellow
        Invoke-Cli @('compile', '--fqbn', $Entry.Fqbn, '--build-path', $buildPath, $Entry.Path) | Out-Null
    }
    $inoName = (Split-Path $Entry.Path -Leaf) + '.ino'
    $db = Get-Content (Join-Path $buildPath 'compile_commands.json') -Raw | ConvertFrom-Json
    $sketchCmd = @($db | Where-Object { $_.file -like "*\sketch\$inoName.cpp" -or $_.file -like "*/sketch/$inoName.cpp" })[0]
    if (-not $sketchCmd) { throw "No compile command found for $inoName" }

    $expanded = Expand-CompilerArgs @($sketchCmd.arguments)
    $compiler = $expanded[0]
    $flags = New-Object System.Collections.Generic.List[string]
    $includeDirs = New-Object System.Collections.Generic.List[string]
    for ($i = 1; $i -lt $expanded.Count; $i++) {
        $a = $expanded[$i]
        if ($a -eq '-o') { $i++; continue }
        if ($a -in @('-c', '-MMD', '-MD') -or $a -eq $sketchCmd.file) { continue }
        if ($a.StartsWith('-I')) { $includeDirs.Add($a.Substring(2)) }
        $flags.Add($a)
    }

    # The .ino has no #include <Arduino.h>; arduino-cli adds it when generating .ino.cpp, so force-include it here.
    $arduinoH = $includeDirs | ForEach-Object { Join-Path $_ 'Arduino.h' } | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    $inoPath = Join-Path $Entry.Path $inoName
    $arguments = @($compiler) + $flags.ToArray() + @('-x', 'c++')
    if ($arduinoH) { $arguments += @('-include', $arduinoH) }
    $arguments += $inoPath
    [pscustomobject]@{ directory = $Entry.Path; file = $inoPath; arguments = $arguments }
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
        elseif ($entry.Fqbn -like 'arduino:avr:nano:*') {
            Write-Host 'Nano: "not in sync" usually means the other bootloader; retry with -Fqbn arduino:avr:nano:cpu=atmega328old (or atmega328). If the upload hangs, unplug/replug the Nano.' -ForegroundColor Yellow
        }
        Write-Host "=== Uploading $($entry.Name) [$($entry.Fqbn)] to $Port" -ForegroundColor Cyan
        Invoke-Cli @('compile', '--fqbn', $entry.Fqbn, '--upload', '--port', $Port, $entry.Path)
    }

    'intellisense' {
        $dbPath = Join-Path $repoRoot '.build\compile_commands.json'
        $names = if (-not $Sketch -or $Sketch -eq 'all') { @($config.sketches.PSObject.Properties.Name) } else { @($Sketch) }
        $entries = @{}
        if ($names.Count -lt $config.sketches.PSObject.Properties.Name.Count -and (Test-Path $dbPath)) {
            # Assign first: Windows PowerShell 5.1 emits a parsed JSON array as a single pipeline object.
            $existing = Get-Content $dbPath -Raw | ConvertFrom-Json
            foreach ($e in $existing) {
                if (Test-Path -LiteralPath $e.file) { $entries[$e.file] = $e }
            }
        }
        foreach ($name in $names) {
            $entry = Get-SketchEntry $name
            Write-Host "=== IntelliSense flags for $($entry.Name) [$($entry.Fqbn)]" -ForegroundColor Cyan
            $item = New-IntelliSenseEntry $entry
            $entries[$item.file] = $item
        }
        New-Item -ItemType Directory -Force (Split-Path $dbPath) | Out-Null
        $json = ConvertTo-Json -InputObject @($entries.Values | Sort-Object file) -Depth 5
        [IO.File]::WriteAllText($dbPath, $json, (New-Object System.Text.UTF8Encoding($false)))
        Write-Host "Wrote $($entries.Count) entries to $dbPath" -ForegroundColor Green
        Write-Host 'If VS Code does not pick up the change, run "C/C++: Reset IntelliSense Database".' -ForegroundColor Yellow
    }

    'monitor' {
        if (-not $Port) {
            if (-not $Sketch) { throw 'Pass -Port COMx (or a sketch name to auto-detect the port).' }
            $Port = Find-Port (Get-SketchEntry $Sketch).Fqbn
        }
        Invoke-Cli @('monitor', '--port', $Port, '--config', "baudrate=$Baud")
    }
}
