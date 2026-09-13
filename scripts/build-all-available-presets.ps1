[CmdletBinding()]
param(
    [switch]$RunTests
)

$ErrorActionPreference = 'Stop'

function Get-PresetNamesFromListOutput {
    param(
        [Parameter(Mandatory = $true)][object]$Lines
    )

    $names = @()
    foreach ($line in @($Lines)) {
        $text = "$line".TrimEnd()
        if ([string]::IsNullOrWhiteSpace($text)) {
            continue
        }

        if ($text -match '^\s+"(?<name>[^"]+)"\s+-\s+') {
            $names += $Matches['name']
        }
    }
    return $names
}

function Invoke-ConfigureAndBuildPreset {
    param(
        [Parameter(Mandatory = $true)][string]$ConfigurePreset,
        [Parameter(Mandatory = $true)][string]$BuildPreset
    )

    Write-Host "==> Configure: $ConfigurePreset" -ForegroundColor Cyan
    cmake --preset $ConfigurePreset

    Write-Host "==> Build: $BuildPreset" -ForegroundColor Cyan
    cmake --build --preset $BuildPreset
}

function Invoke-TestPreset {
    param(
        [Parameter(Mandatory = $true)][string]$TestPreset
    )

    Write-Host "==> Test: $TestPreset" -ForegroundColor Cyan
    ctest --preset $TestPreset
}

# Load preset mapping from file (build preset -> configure preset)
$root = Resolve-Path (Join-Path $PSScriptRoot '..')
$buildRoot = Join-Path $root 'build'
$null = New-Item -ItemType Directory -Force -Path $buildRoot

$presetsPath = Join-Path $root 'CMakePresets.json'
if (-not (Test-Path $presetsPath)) {
    throw "CMakePresets.json not found at: $presetsPath"
}

$presetsJson = Get-Content $presetsPath -Raw | ConvertFrom-Json
$buildToConfigure = @{}
foreach ($bp in ($presetsJson.buildPresets | Where-Object { $_ -ne $null })) {
    if ($bp.name -and $bp.configurePreset) {
        $buildToConfigure[$bp.name] = $bp.configurePreset
    }
}

Push-Location $root
try {
    Write-Host 'Discovering available build presets...' -ForegroundColor Yellow
    $buildList = (& cmake --build --list-presets 2>&1)
    $availableBuildPresets = Get-PresetNamesFromListOutput -Lines $buildList

    if (-not $availableBuildPresets -or $availableBuildPresets.Count -eq 0) {
        throw 'No build presets were discovered. Run "cmake --build --list-presets" to inspect output.'
    }

    # Configure/build in a stable order
    $availableBuildPresets = $availableBuildPresets | Sort-Object

    $configured = New-Object 'System.Collections.Generic.HashSet[string]'
    $failedConfigure = New-Object 'System.Collections.Generic.HashSet[string]'
    $failures = New-Object System.Collections.Generic.List[string]

    foreach ($buildPreset in $availableBuildPresets) {
        if (-not $buildToConfigure.ContainsKey($buildPreset)) {
            throw "Build preset '$buildPreset' does not specify configurePreset in CMakePresets.json"
        }

        $configurePreset = $buildToConfigure[$buildPreset]

        if ($failedConfigure.Contains($configurePreset)) {
            Write-Host "==> Skip build (configure failed): $buildPreset" -ForegroundColor DarkYellow
            continue
        }

        if (-not $configured.Contains($configurePreset)) {
            Write-Host "(once) Configure preset: $configurePreset" -ForegroundColor DarkGray
            & cmake --preset $configurePreset
            if ($LASTEXITCODE -ne 0) {
                $failures.Add("Configure failed: $configurePreset (exit $LASTEXITCODE)")
                [void]$failedConfigure.Add($configurePreset)
                continue
            }
            [void]$configured.Add($configurePreset)
        }

        Write-Host "==> Build preset: $buildPreset" -ForegroundColor Cyan
        & cmake --build --preset $buildPreset
        if ($LASTEXITCODE -ne 0) {
            $failures.Add("Build failed: $buildPreset (exit $LASTEXITCODE)")
        }
    }

    if ($RunTests) {
        Write-Host 'Discovering available test presets...' -ForegroundColor Yellow
        $testList = (& ctest --list-presets 2>&1)
        $availableTestPresets = Get-PresetNamesFromListOutput -Lines $testList | Sort-Object

        foreach ($testPreset in $availableTestPresets) {
            & ctest --preset $testPreset
            if ($LASTEXITCODE -ne 0) {
                $failures.Add("Tests failed: $testPreset (exit $LASTEXITCODE)")
            }
        }
    }

    if ($failures.Count -gt 0) {
        Write-Host ''
        Write-Host 'Failures:' -ForegroundColor Red
        foreach ($failure in $failures) {
            Write-Host "- $failure" -ForegroundColor Red
        }
        exit 1
    }
}
finally {
    Pop-Location
}

Write-Host 'All available preset builds completed.' -ForegroundColor Green
